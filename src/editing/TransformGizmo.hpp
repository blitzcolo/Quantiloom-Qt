/**
 * @file TransformGizmo.hpp
 * @brief Transform manipulation tool for scene objects
 *
 * Handles translate/rotate/scale operations via mouse dragging.
 * Supports axis constraints and coordinate space switching.
 *
 * UX Design (the bindings are registered as QActions by MainWindow; this list
 * used to say W/E/R, which is not what the code has ever dispatched):
 * - G: Translate mode
 * - R: Rotate mode
 * - T: Scale mode
 * - X/Y/Z: Constrain to axis (toggle)
 * - Shift: Fine control (10x slower)
 * - Space: Toggle world/local coordinates
 *
 * @author blitzccolo
 */

#pragma once

#include <QObject>
#include <QPointF>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class SelectionManager;

namespace quantiloom {
class Scene;
}

/**
 * @class TransformGizmo
 * @brief Virtual transform gizmo for mouse-based object manipulation
 *
 * No visual rendering (yet) - transforms are applied via mouse drag.
 * The class computes delta transforms based on:
 * - Current mode (translate/rotate/scale)
 * - Active axis constraints
 * - Mouse delta and camera orientation
 */
class TransformGizmo : public QObject {
    Q_OBJECT

public:
    enum class Mode {
        Translate,
        Rotate,
        Scale
    };

    enum class Space {
        World,
        Local
    };

    // Axis flags (can be combined)
    enum Axis : int {
        None = 0,
        X = 1 << 0,
        Y = 1 << 1,
        Z = 1 << 2,
        XY = X | Y,
        XZ = X | Z,
        YZ = Y | Z,
        XYZ = X | Y | Z
    };

    explicit TransformGizmo(QObject* parent = nullptr);

    // Mode control
    void setMode(Mode mode);
    [[nodiscard]] Mode mode() const { return m_mode; }

    // Space control
    void setSpace(Space space);
    void toggleSpace();
    [[nodiscard]] Space space() const { return m_space; }

    // Axis constraint
    void setAxisConstraint(Axis axis);
    void toggleAxisConstraint(Axis axis);
    [[nodiscard]] Axis axisConstraint() const { return m_axisConstraint; }

    // Fine control (Shift key)
    void setFineControl(bool fine) { m_fineControl = fine; }
    [[nodiscard]] bool fineControl() const { return m_fineControl; }

    // Drag handling
    void beginDrag(const QPointF& screenPos, const glm::vec3& cameraPos,
                   const glm::vec3& cameraForward, const glm::vec3& cameraRight,
                   const glm::vec3& cameraUp);
    void updateDrag(const QPointF& screenPos);
    void endDrag();
    [[nodiscard]] bool isDragging() const { return m_isDragging; }

    // Set pivot point (center of selected objects)
    void setPivot(const glm::vec3& pivot) { m_pivot = pivot; }
    [[nodiscard]] const glm::vec3& pivot() const { return m_pivot; }

    // Set initial transform (for local space operations)
    void setInitialTransform(const glm::mat4& transform);

    // Get accumulated delta transform since drag began
    [[nodiscard]] glm::vec3 deltaTranslation() const { return m_deltaTranslation; }
    [[nodiscard]] glm::quat deltaRotation() const { return m_deltaRotation; }
    [[nodiscard]] glm::vec3 deltaScale() const { return m_deltaScale; }

    // Apply delta to a transform matrix
    [[nodiscard]] glm::mat4 applyDelta(const glm::mat4& original) const;

    // Sensitivity settings
    void setTranslateSensitivity(float s) { m_translateSensitivity = s; }
    void setRotateSensitivity(float s) { m_rotateSensitivity = s; }
    void setScaleSensitivity(float s) { m_scaleSensitivity = s; }

signals:
    // Emitted when mode changes
    void modeChanged(Mode mode);

    // Emitted when space changes
    void spaceChanged(Space space);

    // Emitted when the axis constraint changes, so the Edit ▸ Transform
    // entries and the toolbar buttons can show which one is active
    void axisConstraintChanged(Axis axis);

    // Emitted during drag with current delta
    void transformChanged(const glm::vec3& translation,
                          const glm::quat& rotation,
                          const glm::vec3& scale);

    // Emitted when drag ends
    void transformFinished();

private:
    glm::vec3 applyAxisConstraint(const glm::vec3& delta) const;
    glm::vec3 screenToWorldDelta(const QPointF& screenDelta) const;

    Mode m_mode = Mode::Translate;
    Space m_space = Space::World;
    Axis m_axisConstraint = Axis::XYZ;

    bool m_isDragging = false;
    bool m_fineControl = false;

    QPointF m_dragStart;
    QPointF m_lastDragPos;

    glm::vec3 m_pivot{0.0f};
    glm::mat4 m_initialTransform{1.0f};

    // Camera orientation for screen-to-world conversion
    glm::vec3 m_cameraPos{0.0f, 0.0f, 5.0f};
    glm::vec3 m_cameraForward{0.0f, 0.0f, -1.0f};
    glm::vec3 m_cameraRight{1.0f, 0.0f, 0.0f};
    glm::vec3 m_cameraUp{0.0f, 1.0f, 0.0f};

    // Accumulated delta during drag
    glm::vec3 m_deltaTranslation{0.0f};
    glm::quat m_deltaRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_deltaScale{1.0f};

    // Sensitivity
    float m_translateSensitivity = 0.05f;  // Increased for visibility
    float m_rotateSensitivity = 0.5f;      // Degrees per pixel
    float m_scaleSensitivity = 0.01f;      // Increased for visibility
};
