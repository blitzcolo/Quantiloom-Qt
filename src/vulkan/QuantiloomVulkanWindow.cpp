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

#include <core/Image.hpp>

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

uint32_t QuantiloomVulkanWindow::targetSPP() const {
    return m_renderer ? m_renderer->targetSPP() : 0;
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

void QuantiloomVulkanWindow::setAtmosphericPreset(const QString& preset) {
    withRenderer([preset](QuantiloomVulkanRenderer& r) { r.setAtmosphericPreset(preset); });
}

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
        qDebug() << "QuantiloomVulkanWindow::setNodeTransform - node:" << nodeIndex;
        ctx->SetNodeTransform(static_cast<quantiloom::u32>(nodeIndex), transform);
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
    // Escape stays here: it means "cancel this drag, then clear the
    // selection", which is about the state of this window and has no menu
    // equivalent.
    if (m_editMode && event->key() == Qt::Key_Escape) {
        if (m_transformDragging && m_gizmo && m_gizmo->isDragging()) {
            m_gizmo->endDrag();
            m_transformDragging = false;
        }
        if (m_selection) {
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
            // Edit mode: Left click for selection or transform start
            if (m_selection && m_selection->hasSelection() && m_gizmo) {
                // Start transform drag
                m_transformDragging = true;
                m_transformDragStart = toDevicePixels(event->position());

                qDebug() << "Starting transform drag - hasSelection:" << m_selection->hasSelection()
                         << "count:" << m_selection->selectionCount();

                glm::vec3 camPos, camFwd, camRight, camUp;
                getCameraInfo(camPos, camFwd, camRight, camUp);

                // Set pivot at selection center
                const auto* scene = getScene();
                if (scene) {
                    glm::vec3 pivot = m_selection->computeSelectionCenter(scene);
                    m_gizmo->setPivot(pivot);
                    qDebug() << "  Pivot:" << pivot.x << pivot.y << pivot.z;
                }

                m_gizmo->beginDrag(m_transformDragStart, camPos, camFwd, camRight, camUp);
            } else {
                qDebug() << "No selection - emitting viewportClicked";
                // Click for selection
                emit viewportClicked(device);
            }
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_mousePressed = true;
        m_lastMousePos = toDevicePixels(event->position());
        event->accept();
    } else {
        QVulkanWindow::mousePressEvent(event);
    }
}

void QuantiloomVulkanWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_transformDragging) {
        m_transformDragging = false;
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
    if (m_transformDragging && m_gizmo && m_gizmo->isDragging()) {
        m_gizmo->updateDrag(toDevicePixels(event->position()));
        event->accept();
        return;
    }

    if (m_mousePressed && m_renderer) {
        const QPointF device = toDevicePixels(event->position());
        QPointF delta = device - m_lastMousePos;
        m_lastMousePos = device;

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
        // Don't accept - let base class handle too
    }
    return QVulkanWindow::event(event);
}
