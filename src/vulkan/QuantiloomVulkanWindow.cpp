/**
 * @file QuantiloomVulkanWindow.cpp
 * @brief QVulkanWindow subclass implementation
 *
 * @author blitzcolo
 */

#include "QuantiloomVulkanWindow.hpp"
#include "QuantiloomVulkanRenderer.hpp"
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
#include <QDebug>

#include <renderer/ExternalRenderContext.hpp>
#include <renderer/LightingParams.hpp>
#include <postprocess/SensorModel.hpp>
#include <scene/Material.hpp>
#include <scene/Scene.hpp>

QuantiloomVulkanWindow::QuantiloomVulkanWindow(QWindow* parent)
    : QVulkanWindow(parent)
{
    // The SDK presents by blitting its linear R32G32B32A32_SFLOAT output straight
    // into the swapchain image, with no shader in between. A blit applies the
    // destination format's transfer function, so an sRGB target is what performs
    // the linear->sRGB encode -- Qt's default UNORM target would display linear
    // radiance uncorrected, which is the whole viewport a stop and a half too dark
    // against the CLI's PNG. Qt falls back to its default if neither is supported.
    setPreferredColorFormats({VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB});

    // Request required device extensions for ray tracing
    setDeviceExtensions({
        // Ray tracing core extensions
        "VK_KHR_acceleration_structure",
        "VK_KHR_ray_tracing_pipeline",
        "VK_KHR_ray_query",  // Required if shaders use RayQuery capability
        "VK_KHR_deferred_host_operations",
        // Required by ray tracing
        "VK_KHR_buffer_device_address",
        "VK_KHR_spirv_1_4",
        "VK_KHR_shader_float_controls",
        // Dynamic rendering (Vulkan 1.3 core, but request as extension for compatibility)
        "VK_KHR_dynamic_rendering",
        // Synchronization2 (Vulkan 1.3 core)
        "VK_KHR_synchronization2",
        // Maintenance extensions often required
        "VK_KHR_maintenance3",
        "VK_KHR_maintenance4",
        // Descriptor indexing for bindless textures
        "VK_EXT_descriptor_indexing",
        // Scalar block layout
        "VK_EXT_scalar_block_layout"
    });

    // Enable required Vulkan features for ray tracing using Qt 6.7+ API
    // Reference: https://doc.qt.io/qt-6/qvulkanwindow.html#setEnabledFeaturesModifier
    setEnabledFeaturesModifier([this](VkPhysicalDeviceFeatures2& features) {
        qDebug() << "QuantiloomVulkanWindow: Enabling ray tracing device features...";

        // Initialize feature structures with sType
        m_bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        m_bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

        m_accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        m_accelerationStructureFeatures.accelerationStructure = VK_TRUE;

        m_rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        m_rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;

        m_rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
        m_rayQueryFeatures.rayQuery = VK_TRUE;

        m_dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
        m_dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

        m_synchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
        m_synchronization2Features.synchronization2 = VK_TRUE;

        m_descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        m_descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        m_descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
        m_descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
        m_descriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;

        m_scalarBlockLayoutFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES;
        m_scalarBlockLayoutFeatures.scalarBlockLayout = VK_TRUE;

        // Build pNext chain (matches VulkanContext order):
        // features -> bufferDeviceAddress -> accelerationStructure -> rayTracingPipeline
        //          -> rayQuery -> dynamicRendering -> synchronization2
        //          -> descriptorIndexing -> scalarBlockLayout
        features.pNext = &m_bufferDeviceAddressFeatures;
        m_bufferDeviceAddressFeatures.pNext = &m_accelerationStructureFeatures;
        m_accelerationStructureFeatures.pNext = &m_rayTracingPipelineFeatures;
        m_rayTracingPipelineFeatures.pNext = &m_rayQueryFeatures;
        m_rayQueryFeatures.pNext = &m_dynamicRenderingFeatures;
        m_dynamicRenderingFeatures.pNext = &m_synchronization2Features;
        m_synchronization2Features.pNext = &m_descriptorIndexingFeatures;
        m_descriptorIndexingFeatures.pNext = &m_scalarBlockLayoutFeatures;
        m_scalarBlockLayoutFeatures.pNext = nullptr;

        // Enable required Vulkan 1.0 features
        features.features.shaderInt64 = VK_TRUE;
        features.features.samplerAnisotropy = VK_TRUE;

        qDebug() << "  Ray tracing features enabled via pNext chain (including rayQuery, synchronization2)";
    });

    qDebug() << "QuantiloomVulkanWindow: Requested ray tracing device extensions";
}

void QuantiloomVulkanWindow::selectRayTracingDevice() {
    // Qt takes device 0 unless told otherwise. On a laptop that is often the
    // integrated GPU, which has no ray tracing at all, and the failure lands
    // much later as a null VkDevice with nothing pointing at the cause.
    const QList<VkPhysicalDeviceProperties> devices = availablePhysicalDevices();
    if (devices.size() <= 1) {
        return;  // No choice to make; leave Qt's default alone.
    }

    int chosen = -1;
    for (int i = 0; i < devices.size(); ++i) {
        qDebug() << "  Vulkan device" << i << ":" << devices[i].deviceName
                 << "type" << devices[i].deviceType;
        // First discrete GPU wins. Ray tracing support proper cannot be
        // queried here -- QVulkanWindow exposes only VkPhysicalDeviceProperties
        // -- but a discrete GPU is the right guess on every machine where the
        // two differ, and Qt still validates the extensions when it creates
        // the device.
        if (chosen < 0 && devices[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            chosen = i;
        }
    }

    if (chosen >= 0) {
        qDebug() << "QuantiloomVulkanWindow: selecting discrete GPU"
                 << devices[chosen].deviceName;
        setPhysicalDeviceIndex(chosen);
    }
}

QuantiloomVulkanWindow::~QuantiloomVulkanWindow() = default;

QVulkanWindowRenderer* QuantiloomVulkanWindow::createRenderer() {
    m_renderer = new QuantiloomVulkanRenderer(this);

    // The queued settings are *not* replayed here: there is no render context
    // yet. The renderer calls applyDeferredSettings() once there is one.

    // Load pending scene if set before renderer was created
    if (!m_pendingScenePath.isEmpty()) {
        m_renderer->loadScene(m_pendingScenePath);
        m_pendingScenePath.clear();
    }

    return m_renderer;
}

void QuantiloomVulkanWindow::applyDeferredSettings() {
    if (!m_renderer || m_deferredCalls.empty()) {
        return;
    }
    // Settings first, scene second: the renderer keeps each setting as a
    // member and re-applies it after a scene loads, so this order is the one
    // that survives the load.
    const auto calls = std::move(m_deferredCalls);
    m_deferredCalls.clear();
    for (const auto& call : calls) {
        call(*m_renderer);
    }
}

void QuantiloomVulkanWindow::withRenderer(std::function<void(QuantiloomVulkanRenderer&)> call) {
    if (m_renderer) {
        call(*m_renderer);
        return;
    }
    m_deferredCalls.push_back(std::move(call));
}

void QuantiloomVulkanWindow::loadScene(const QString& filePath) {
    if (m_renderer) {
        m_renderer->loadScene(filePath);
    } else {
        // Store for later loading when renderer is created
        m_pendingScenePath = filePath;
    }
}

void QuantiloomVulkanWindow::applyConfig(std::shared_ptr<const quantiloom::Config> config,
                                         const QString& baseDir) {
    // Through withRenderer() rather than a second pending-path member: the
    // renderer holds the config either way and replays it once it has a
    // context, so there is nothing for this class to remember.
    withRenderer([config = std::move(config), baseDir](QuantiloomVulkanRenderer& renderer) {
        renderer.applyConfig(config, baseDir);
    });
}

void QuantiloomVulkanWindow::resetCamera() {
    withRenderer([](QuantiloomVulkanRenderer& r) { r.resetCamera(); });
}

void QuantiloomVulkanWindow::setCamera(const glm::vec3& position, const glm::vec3& lookAt,
                                        const glm::vec3& up, float fovY) {
    withRenderer([position, lookAt, up, fovY](QuantiloomVulkanRenderer& r) {
        r.setCamera(position, lookAt, up, fovY);
    });
}

void QuantiloomVulkanWindow::getCameraState(glm::vec3& position, glm::vec3& target,
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

void QuantiloomVulkanWindow::setCameraFovY(float fovY) {
    withRenderer([fovY](QuantiloomVulkanRenderer& r) { r.setCameraFovY(fovY); });
}

void QuantiloomVulkanWindow::setViewDirection(const glm::vec3& direction) {
    withRenderer([direction](QuantiloomVulkanRenderer& r) { r.setViewDirection(direction); });
}

void QuantiloomVulkanWindow::setSPP(uint32_t spp) {
    withRenderer([spp](QuantiloomVulkanRenderer& r) { r.setSPP(spp); });
}

void QuantiloomVulkanWindow::setSamplingSeed(uint32_t seed) {
    withRenderer([seed](QuantiloomVulkanRenderer& r) { r.setSamplingSeed(seed); });
}

void QuantiloomVulkanWindow::setWavelength(float wavelength_nm) {
    withRenderer([wavelength_nm](QuantiloomVulkanRenderer& r) { r.setWavelength(wavelength_nm); });
}

uint32_t QuantiloomVulkanWindow::currentSampleCount() const {
    return m_renderer ? m_renderer->currentSampleCount() : 0;
}

void QuantiloomVulkanWindow::frameBounds(const glm::vec3& min, const glm::vec3& max) {
    if (m_renderer) {
        m_renderer->frameBounds(min, max);
    }
}

bool QuantiloomVulkanWindow::sensorEnabled() const {
    return m_renderer && m_renderer->isSensorEnabled();
}

quantiloom::SensorParams QuantiloomVulkanWindow::sensorParams() const {
    return m_renderer ? m_renderer->getSensorParams() : quantiloom::SensorParams{};
}

void QuantiloomVulkanWindow::setCameraProjection(bool orthographic, float orthoHeight) {
    if (m_renderer) {
        m_renderer->setCameraProjection(orthographic, orthoHeight);
    }
}

bool QuantiloomVulkanWindow::cameraIsOrthographic() const {
    return m_renderer && m_renderer->cameraIsOrthographic();
}

float QuantiloomVulkanWindow::cameraOrthoHeight() const {
    return m_renderer ? m_renderer->cameraOrthoHeight() : 2.0f;
}

void QuantiloomVulkanWindow::setSceneScale(float radius) {
    if (m_renderer) {
        m_renderer->setSceneScale(radius);
    }
}

float QuantiloomVulkanWindow::lastGpuFrameTimeMs() const {
    return m_renderer ? m_renderer->lastGpuFrameTimeMs() : 0.0f;
}

quantiloom::LightingParams QuantiloomVulkanWindow::lightingParams() const {
    return m_renderer ? m_renderer->lightingParams()
                      : quantiloom::CreateDefaultLightingParams();
}

quantiloom::SpectralMode QuantiloomVulkanWindow::spectralMode() const {
    return m_renderer ? m_renderer->spectralMode() : quantiloom::SpectralMode::RGB;
}

float QuantiloomVulkanWindow::wavelength() const {
    return m_renderer ? m_renderer->wavelength() : 550.0f;
}

quantiloom::AtmosphereNNConfig QuantiloomVulkanWindow::atmosphericConfig() const {
    return m_renderer ? m_renderer->atmosphericConfig() : quantiloom::AtmosphereNNConfig{};
}

quantiloom::DebugVisualizationMode QuantiloomVulkanWindow::debugMode() const {
    return m_renderer ? m_renderer->getDebugMode() : quantiloom::DebugVisualizationMode::None;
}

uint32_t QuantiloomVulkanWindow::targetSPP() const {
    return m_renderer ? m_renderer->targetSPP() : 0;
}

uint32_t QuantiloomVulkanWindow::samplingSeed() const {
    return m_renderer ? m_renderer->samplingSeed() : quantiloom::constants::DEFAULT_SAMPLING_SEED;
}

void QuantiloomVulkanWindow::setRenderPaused(bool paused) {
    withRenderer([paused](QuantiloomVulkanRenderer& r) { r.setPaused(paused); });
}

bool QuantiloomVulkanWindow::isRenderPaused() const {
    return m_renderer && m_renderer->isPaused();
}

QPointF QuantiloomVulkanWindow::toDevicePixels(const QPointF& logical) const {
    const qreal ratio = devicePixelRatio();
    return QPointF(logical.x() * ratio, logical.y() * ratio);
}

void QuantiloomVulkanWindow::setSpectralMode(quantiloom::SpectralMode mode) {
    withRenderer([mode](QuantiloomVulkanRenderer& r) { r.setSpectralMode(mode); });
}

void QuantiloomVulkanWindow::setDebugMode(quantiloom::DebugVisualizationMode mode) {
    withRenderer([mode](QuantiloomVulkanRenderer& r) { r.setDebugMode(mode); });
}

void QuantiloomVulkanWindow::setLightingParams(const quantiloom::LightingParams& params) {
    withRenderer([params](QuantiloomVulkanRenderer& r) { r.setLightingParams(params); });
}

void QuantiloomVulkanWindow::updateMaterial(int index, const quantiloom::Material& material) {
    if (m_renderer) {
        m_renderer->updateMaterial(index, material);
    }
}

int QuantiloomVulkanWindow::addComplexRefractiveIndex(const quantiloom::ComplexRefractiveIndex& cri) {
    if (m_renderer)
        return m_renderer->addComplexRefractiveIndex(cri);
    return -1;
}

int QuantiloomVulkanWindow::addSpectralCurve(const quantiloom::SpectralCurve& curve) {
    if (m_renderer)
        return m_renderer->addSpectralCurve(curve);
    return -1;
}

std::optional<QString> QuantiloomVulkanWindow::setSolarLutFromSpec(
    const quantiloom::SolarLutSpec& spec, const QString& baseDir) {
    if (!m_renderer) {
        return QObject::tr("The renderer is not ready yet.");
    }
    return m_renderer->setSolarLutFromSpec(spec, baseDir);
}

void QuantiloomVulkanWindow::setSolarSpectralLUT(const quantiloom::SpectralCurve& sun,
                                                 const quantiloom::SpectralCurve& sky) {
    if (m_renderer)
        m_renderer->setSolarSpectralLUT(sun, sky);
}

void QuantiloomVulkanWindow::resetAccumulation() {
    if (m_renderer) {
        m_renderer->resetAccumulation();
    }
}

const quantiloom::Scene* QuantiloomVulkanWindow::getScene() const {
    return m_renderer ? m_renderer->getScene() : nullptr;
}

bool QuantiloomVulkanWindow::readDebugPixel(int x, int y, glm::vec4& outValue) {
    return m_renderer ? m_renderer->readDebugPixel(x, y, outValue) : false;
}

QString QuantiloomVulkanWindow::formatDebugValue(const glm::vec4& pixel) const {
    return m_renderer ? m_renderer->formatDebugValue(pixel) : QString("--");
}

quantiloom::DebugVisualizationMode QuantiloomVulkanWindow::getDebugMode() const {
    return m_renderer ? m_renderer->getDebugMode() : quantiloom::DebugVisualizationMode::None;
}

std::unique_ptr<quantiloom::Image> QuantiloomVulkanWindow::captureScreenshot() {
    return m_renderer ? m_renderer->captureScreenshot() : nullptr;
}

std::unique_ptr<quantiloom::Image> QuantiloomVulkanWindow::captureDisplayImage() {
    return m_renderer ? m_renderer->captureDisplayImage() : nullptr;
}

// ============================================================================
// Atmospheric Configuration
// ============================================================================

void QuantiloomVulkanWindow::setAtmosphericConfig(const quantiloom::AtmosphereNNConfig& config) {
    withRenderer([config](QuantiloomVulkanRenderer& r) { r.setAtmosphericConfig(config); });
}

// ============================================================================
// Environment Map (IBL)
// ============================================================================

bool QuantiloomVulkanWindow::loadEnvironmentMap(const QString& hdrPath) {
    if (!m_renderer) {
        // Queued rather than refused. Reporting false here would have the
        // caller log a failure for a map that does load moments later.
        withRenderer([hdrPath](QuantiloomVulkanRenderer& r) { r.loadEnvironmentMap(hdrPath); });
        return true;
    }
    return m_renderer->loadEnvironmentMap(hdrPath);
}

// ============================================================================
// Sensor Simulation
// ============================================================================

void QuantiloomVulkanWindow::setSensorEnabled(bool enabled) {
    withRenderer([enabled](QuantiloomVulkanRenderer& r) { r.setSensorEnabled(enabled); });
}

void QuantiloomVulkanWindow::setSensorParams(const quantiloom::SensorParams& params) {
    withRenderer([params](QuantiloomVulkanRenderer& r) { r.setSensorParams(params); });
}

void QuantiloomVulkanWindow::setDisplayEnhancement(bool enabled, float clipLimit,
                                                    int tileSize, bool luminanceOnly) {
    withRenderer([enabled, clipLimit, tileSize, luminanceOnly](QuantiloomVulkanRenderer& r) {
        r.setDisplayEnhancement(enabled, clipLimit, tileSize, luminanceOnly);
    });
}

// ============================================================================
// Viewport Overlay
// ============================================================================

void QuantiloomVulkanWindow::setGridVisible(bool visible) {
    withRenderer([visible](QuantiloomVulkanRenderer& r) { r.setGridVisible(visible); });
}

bool QuantiloomVulkanWindow::isGridVisible() const {
    return m_renderer && m_renderer->isGridVisible();
}

// ============================================================================
// Scene Editing
// ============================================================================

void QuantiloomVulkanWindow::setEditingComponents(SelectionManager* selection,
                                                   TransformGizmo* gizmo,
                                                   UndoStack* undoStack) {
    m_selection = selection;
    m_gizmo = gizmo;
    m_undoStack = undoStack;
}

void QuantiloomVulkanWindow::setNodeTransform(int nodeIndex, const glm::mat4& transform) {
    if (!m_renderer) return;

    auto* ctx = m_renderer->getRenderContext();
    if (ctx && nodeIndex >= 0) {
        ctx->SetNodeTransform(static_cast<quantiloom::u32>(nodeIndex), transform);
        ctx->RebuildAccelerationStructure();
        m_renderer->resetAccumulation();
    }
}

void QuantiloomVulkanWindow::setNodeTransformInteractive(int nodeIndex,
                                                         const glm::mat4& transform) {
    if (!m_renderer) return;
    auto* ctx = m_renderer->getRenderContext();
    if (ctx && nodeIndex >= 0) {
        // Transform only -- the caller batches all moved nodes, then refits
        // once per mouse move via refitAfterInteractiveEdit()
        ctx->SetNodeTransform(static_cast<quantiloom::u32>(nodeIndex), transform);
    }
}

void QuantiloomVulkanWindow::refitAfterInteractiveEdit() {
    if (!m_renderer) return;
    auto* ctx = m_renderer->getRenderContext();
    if (ctx) {
        // A refit updates the TLAS in place -- no allocation, no device idle
        // -- which is what keeps a drag at interactive framerates
        ctx->RefitAccelerationStructure();
        m_renderer->resetAccumulation();
    }
}

int QuantiloomVulkanWindow::duplicateNode(int sourceIndex, const QString& newName) {
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

bool QuantiloomVulkanWindow::removeNode(int nodeIndex) {
    if (!m_renderer || nodeIndex < 0) return false;
    auto* ctx = m_renderer->getRenderContext();
    return ctx && ctx->RemoveNode(static_cast<quantiloom::u32>(nodeIndex));
}

bool QuantiloomVulkanWindow::restoreNode(int nodeIndex) {
    if (!m_renderer || nodeIndex < 0) return false;
    auto* ctx = m_renderer->getRenderContext();
    return ctx && ctx->RestoreNode(static_cast<quantiloom::u32>(nodeIndex));
}

void QuantiloomVulkanWindow::rebuildSceneTopology() {
    if (!m_renderer) return;
    auto* ctx = m_renderer->getRenderContext();
    if (ctx) {
        // The instance count changed, so this is always the full rebuild;
        // a refit cannot add or drop instances
        ctx->RebuildAccelerationStructure();
        m_renderer->resetAccumulation();
    }
}

void QuantiloomVulkanWindow::finalizeInteractiveEdit() {
    if (!m_renderer) return;
    auto* ctx = m_renderer->getRenderContext();
    if (ctx) {
        // Full-quality rebuild once, when the drag ends (a refit degrades
        // trace quality slightly for large movements)
        ctx->RebuildAccelerationStructure();
        m_renderer->resetAccumulation();
    }
}

void QuantiloomVulkanWindow::getCameraInfo(glm::vec3& position, glm::vec3& forward,
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

void QuantiloomVulkanWindow::setEditMode(bool edit) {
    if (m_editMode != edit) {
        m_editMode = edit;
        emit editModeChanged(edit);
    }
}

// ============================================================================
// Input Event Handlers
// ============================================================================

void QuantiloomVulkanWindow::keyPressEvent(QKeyEvent* event) {
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
            QVulkanWindow::keyPressEvent(event);
            return;
    }

    if (m_renderer) {
        m_renderer->updateCameraMovement(
            m_keyW, m_keyS, m_keyA, m_keyD, m_keyQ, m_keyE, m_shiftHeld);
    }
}

void QuantiloomVulkanWindow::keyReleaseEvent(QKeyEvent* event) {
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
            QVulkanWindow::keyReleaseEvent(event);
            return;
    }

    if (m_renderer) {
        m_renderer->updateCameraMovement(
            m_keyW, m_keyS, m_keyA, m_keyD, m_keyQ, m_keyE, m_shiftHeld);
    }
}

void QuantiloomVulkanWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // Always emit for debug value display (regardless of edit mode)
        const QPointF device = toDevicePixels(event->position());
        emit mouseHovered(static_cast<int>(device.x()), static_cast<int>(device.y()));

        if (m_editMode) {
            // Three-way dispatch: a gizmo handle under the cursor starts a
            // drag; anything else is a selection click (object or empty
            // space). The old behavior -- any click starting a transform drag
            // as soon as something was selected -- made it impossible to
            // click another object or click empty space to deselect.
            if (m_selection && m_selection->hasSelection() && m_gizmo &&
                beginGizmoDragAt(toDevicePixels(event->position()))) {
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
        event->accept();
    } else {
        QVulkanWindow::mousePressEvent(event);
    }
}

void QuantiloomVulkanWindow::requestCompositedCapture(const QString& path) {
    withRenderer([path](QuantiloomVulkanRenderer& r) { r.requestCompositedCapture(path); });
}

void QuantiloomVulkanWindow::setEditingToolsEnabled(bool enabled) {
    m_editingToolsEnabled = enabled;
    // Switching workspace mid-drag (Ctrl+2 with the mouse held) aborts the
    // drag the same way Escape does: transforms restored, no undo entry
    if (!enabled && m_transformDragging && m_gizmo && m_gizmo->isDragging()) {
        m_gizmo->cancelDrag();
        m_transformDragging = false;
        m_activeHandle = editing::GizmoHandle::None;
    }
    if (!enabled) {
        m_hoveredHandle = editing::GizmoHandle::None;
    }
}

bool QuantiloomVulkanWindow::gizmoOnScreen() const {
    return m_editingToolsEnabled && m_editMode && m_selection &&
           m_selection->hasSelection() && m_gizmo && m_renderer &&
           getScene() != nullptr;
}

editing::GizmoFrame QuantiloomVulkanWindow::currentGizmoFrame(
    const vkview::CameraMatrices& camera) const {
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

bool QuantiloomVulkanWindow::buildGizmoDrawList(
    const vkview::CameraMatrices& camera, std::vector<editing::GizmoVertex>& out) {
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

void QuantiloomVulkanWindow::setSelectionBoxColor(const glm::vec4& color) {
    m_selectionBoxColor = color;
    requestUpdate();
}

bool QuantiloomVulkanWindow::beginGizmoDragAt(const QPointF& devicePos) {
    if (!gizmoOnScreen()) {
        return false;
    }
    const vkview::CameraMatrices camera = m_renderer->overlayCamera();
    const editing::GizmoFrame frame = currentGizmoFrame(camera);
    const vkview::CameraRay ray = camera.rayThroughPixel(
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
    return true;
}

std::optional<quantiloom::PickResult> QuantiloomVulkanWindow::pickScene(
    const QPointF& devicePos) {
    if (!m_renderer) {
        return std::nullopt;
    }
    auto* ctx = m_renderer->getRenderContext();
    if (!ctx || !ctx->HasScene()) {
        return std::nullopt;
    }
    const int x = static_cast<int>(devicePos.x());
    const int y = static_cast<int>(devicePos.y());
    if (x < 0 || y < 0) {
        return std::nullopt;
    }
    auto result = ctx->Pick(static_cast<quantiloom::u32>(x),
                            static_cast<quantiloom::u32>(y));
    if (!result.has_value()) {
        QL_LOG_WARN("pickScene: {}", result.error());
        return std::nullopt;
    }
    return result.value();
}

void QuantiloomVulkanWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_transformDragging) {
        m_transformDragging = false;
        m_activeHandle = editing::GizmoHandle::None;
        if (m_gizmo && m_gizmo->isDragging()) {
            m_gizmo->endDrag();
            // Note: The undo command is pushed in MainWindow when transform finishes
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_mousePressed = false;
        event->accept();
    } else {
        QVulkanWindow::mouseReleaseEvent(event);
    }
}

void QuantiloomVulkanWindow::mouseMoveEvent(QMouseEvent* event) {
    // Transform dragging has priority
    if (m_transformDragging && m_gizmo && m_gizmo->isDragging() && m_renderer) {
        const QPointF device = toDevicePixels(event->position());
        const vkview::CameraRay ray = m_renderer->overlayCamera().rayThroughPixel(
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
        QVulkanWindow::mouseMoveEvent(event);
    }
}

void QuantiloomVulkanWindow::wheelEvent(QWheelEvent* event) {
    if (m_renderer) {
        float delta = event->angleDelta().y() / 120.0f;
        m_renderer->zoomCamera(delta);
        event->accept();
    } else {
        QVulkanWindow::wheelEvent(event);
    }
}

bool QuantiloomVulkanWindow::event(QEvent* event) {
    if (event->type() == QEvent::HoverMove) {
        auto* hoverEvent = static_cast<QHoverEvent*>(event);
        const QPointF device = toDevicePixels(hoverEvent->position());
        emit mouseHovered(static_cast<int>(device.x()), static_cast<int>(device.y()));

        // Gizmo hover highlight: the same hit-test the grab uses, so a handle
        // that lights up is a handle that will respond. Overlay-only state --
        // it never resets accumulation, and the render loop repaints anyway.
        if (!m_transformDragging) {
            editing::GizmoHandle hovered = editing::GizmoHandle::None;
            if (gizmoOnScreen()) {
                const vkview::CameraMatrices camera = m_renderer->overlayCamera();
                const editing::GizmoFrame frame = currentGizmoFrame(camera);
                const vkview::CameraRay ray = camera.rayThroughPixel(
                    static_cast<float>(device.x()), static_cast<float>(device.y()));
                hovered = editing::hitTestGizmo(ray, frame, m_gizmo->mode());
            }
            m_hoveredHandle = hovered;
        }
        // Don't accept - let base class handle too
    }
    return QVulkanWindow::event(event);
}
