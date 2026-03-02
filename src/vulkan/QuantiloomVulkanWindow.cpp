/**
 * @file QuantiloomVulkanWindow.cpp
 * @brief QVulkanWindow subclass implementation
 *
 * @author wtflmao
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

    // Load pending scene if set before renderer was created
    if (!m_pendingScenePath.isEmpty()) {
        m_renderer->loadScene(m_pendingScenePath);
        m_pendingScenePath.clear();
    }

    return m_renderer;
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
    if (m_renderer) {
        m_renderer->resetCamera();
    }
}

void QuantiloomVulkanWindow::setCamera(const glm::vec3& position, const glm::vec3& lookAt,
                                        const glm::vec3& up, float fovY) {
    if (m_renderer) {
        m_renderer->setCamera(position, lookAt, up, fovY);
    }
}

void QuantiloomVulkanWindow::setSPP(uint32_t spp) {
    if (m_renderer) {
        m_renderer->setSPP(spp);
    }
}

void QuantiloomVulkanWindow::setWavelength(float wavelength_nm) {
    if (m_renderer) {
        m_renderer->setWavelength(wavelength_nm);
    }
}

uint32_t QuantiloomVulkanWindow::currentSampleCount() const {
    return m_renderer ? m_renderer->currentSampleCount() : 0;
}

void QuantiloomVulkanWindow::setSpectralMode(quantiloom::SpectralMode mode) {
    if (m_renderer) {
        m_renderer->setSpectralMode(mode);
    }
}

void QuantiloomVulkanWindow::setDebugMode(quantiloom::DebugVisualizationMode mode) {
    if (m_renderer) {
        m_renderer->setDebugMode(mode);
    }
}

void QuantiloomVulkanWindow::setLightingParams(const quantiloom::LightingParams& params) {
    if (m_renderer) {
        m_renderer->setLightingParams(params);
    }
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
    if (m_renderer) {
        m_renderer->setAtmosphericPreset(preset);
    }
}

// ============================================================================
// Environment Map (IBL)
// ============================================================================

bool QuantiloomVulkanWindow::loadEnvironmentMap(const QString& hdrPath) {
    return m_renderer ? m_renderer->loadEnvironmentMap(hdrPath) : false;
}

// ============================================================================
// Sensor Simulation
// ============================================================================

void QuantiloomVulkanWindow::setSensorEnabled(bool enabled) {
    if (m_renderer) {
        m_renderer->setSensorEnabled(enabled);
    }
}

void QuantiloomVulkanWindow::setSensorParams(const quantiloom::SensorParams& params) {
    if (m_renderer) {
        m_renderer->setSensorParams(params);
    }
}

void QuantiloomVulkanWindow::setDisplayEnhancement(bool enabled, float clipLimit,
                                                    int tileSize, bool luminanceOnly) {
    if (m_renderer) {
        m_renderer->setDisplayEnhancement(enabled, clipLimit, tileSize, luminanceOnly);
    }
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
    // Edit mode hotkeys (always active)
    if (m_editMode && m_gizmo) {
        switch (event->key()) {
            case Qt::Key_G:  // Grab/Translate
                m_gizmo->setMode(TransformGizmo::Mode::Translate);
                event->accept();
                return;
            case Qt::Key_R:  // Rotate (not camera rotate)
                if (!(event->modifiers() & Qt::ControlModifier)) {
                    m_gizmo->setMode(TransformGizmo::Mode::Rotate);
                    event->accept();
                    return;
                }
                break;
            case Qt::Key_T:  // Transform/Scale
                m_gizmo->setMode(TransformGizmo::Mode::Scale);
                event->accept();
                return;
            case Qt::Key_X:
                m_gizmo->toggleAxisConstraint(TransformGizmo::Axis::X);
                event->accept();
                return;
            case Qt::Key_Y:
                m_gizmo->toggleAxisConstraint(TransformGizmo::Axis::Y);
                event->accept();
                return;
            case Qt::Key_Z:
                if (!(event->modifiers() & Qt::ControlModifier)) {
                    m_gizmo->toggleAxisConstraint(TransformGizmo::Axis::Z);
                    event->accept();
                    return;
                }
                break;
            case Qt::Key_Space:
                m_gizmo->toggleSpace();
                event->accept();
                return;
            case Qt::Key_Escape:
                if (m_transformDragging && m_gizmo->isDragging()) {
                    m_gizmo->endDrag();
                    m_transformDragging = false;
                    // TODO: Revert transform
                }
                if (m_selection) {
                    m_selection->clearSelection();
                }
                event->accept();
                return;
        }
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
        QPointF pos = event->position();
        emit mouseHovered(static_cast<int>(pos.x()), static_cast<int>(pos.y()));

        if (m_editMode) {
            // Edit mode: Left click for selection or transform start
            if (m_selection && m_selection->hasSelection() && m_gizmo) {
                // Start transform drag
                m_transformDragging = true;
                m_transformDragStart = event->position();

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

                m_gizmo->beginDrag(event->position(), camPos, camFwd, camRight, camUp);
            } else {
                qDebug() << "No selection - emitting viewportClicked";
                // Click for selection
                emit viewportClicked(event->position());
            }
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_mousePressed = true;
        m_lastMousePos = event->position();
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
        m_gizmo->updateDrag(event->position());
        event->accept();
        return;
    }

    if (m_mousePressed && m_renderer) {
        QPointF delta = event->position() - m_lastMousePos;
        m_lastMousePos = event->position();

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
        QPointF pos = hoverEvent->position();
        emit mouseHovered(static_cast<int>(pos.x()), static_cast<int>(pos.y()));
        // Don't accept - let base class handle too
    }
    return QVulkanWindow::event(event);
}
