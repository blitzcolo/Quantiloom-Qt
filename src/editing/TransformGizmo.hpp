/**
 * @file TransformGizmo.hpp
 * @brief Transform manipulation tool for scene objects
 *
 * The modal state machine behind the drawn gizmo: which mode, which space,
 * and — during a drag — how far the grabbed handle has been pulled. The drag
 * math is ray-based: every update intersects the mouse ray with the grabbed
 * handle's axis line or plane and derives the TOTAL delta from the press
 * reference, so the object tracks the cursor exactly at any distance and
 * per-event rounding cannot accumulate.
 *
 * UX (bindings registered as QActions by MainWindow):
 * - G: Translate mode, R: Rotate mode, T: Scale mode
 * - Ctrl: snap (0.5 units / 15 deg / 0.1 scale; with Shift 0.05 / 5 deg)
 * - Shift: fine control (deltas at 10%)
 * - Space: toggle world/local coordinates
 * - Escape: cancel the drag; the shell restores the start transforms
 *
 * @author blitzcolo
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

namespace vkview {
struct CameraRay;
}

namespace editing {
enum class GizmoHandle;
struct GizmoFrame;
}

/**
 * @class TransformGizmo
 * @brief Modal drag state for the viewport transform gizmo
 *
 * Owns no geometry (editing::GizmoModel draws and hit-tests); this class
 * turns "handle H grabbed at ray R0, cursor now at ray R" into a delta
 * transform, applied to each selected node's press-time matrix by
 * applyDelta().
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

    explicit TransformGizmo(QObject* parent = nullptr);

    // Mode control
    void setMode(Mode mode);
    [[nodiscard]] Mode mode() const { return m_mode; }

    // Space control
    void setSpace(Space space);
    void toggleSpace();
    [[nodiscard]] Space space() const { return m_space; }

    // Fine control (Shift key): deltas at 10%
    void setFineControl(bool fine) { m_fineControl = fine; }
    [[nodiscard]] bool fineControl() const { return m_fineControl; }

    // ========================================================================
    // Handle drag
    // ========================================================================

    /// Grab `handle` with the press ray. The frame is the one the handle was
    /// drawn with, so the press reference lands exactly where the cursor is.
    /// Emits dragStarted() -- the shell snapshots start transforms on it.
    void beginHandleDrag(editing::GizmoHandle handle,
                         const vkview::CameraRay& pressRay,
                         const editing::GizmoFrame& frame);

    /// Recompute the total delta from the press reference for the current
    /// mouse ray. `snap` (Ctrl) quantizes the total, not the increment.
    void updateHandleDrag(const vkview::CameraRay& ray, bool snap);

    /// Commit: emits transformFinished() (the shell pushes the undo command).
    void endDrag();

    /// Abort: emits dragCancelled() (the shell restores the start
    /// transforms). No transformFinished, no undo entry.
    void cancelDrag();

    [[nodiscard]] bool isDragging() const { return m_isDragging; }
    [[nodiscard]] editing::GizmoHandle activeHandle() const;

    // Pivot (selection center at press time; world-space rotate/scale center)
    void setPivot(const glm::vec3& pivot) { m_pivot = pivot; }
    [[nodiscard]] const glm::vec3& pivot() const { return m_pivot; }

    // Current total deltas
    [[nodiscard]] glm::vec3 deltaTranslation() const { return m_deltaTranslation; }
    [[nodiscard]] glm::quat deltaRotation() const { return m_deltaRotation; }
    [[nodiscard]] glm::vec3 deltaScale() const { return m_deltaScale; }

    /// The drag's delta applied to a node's press-time transform. Rotation
    /// and scale pivot on the selection center in world space and on the
    /// node's own origin in local space (Blender median-point behavior).
    [[nodiscard]] glm::mat4 applyDelta(const glm::mat4& original) const;

signals:
    void modeChanged(Mode mode);
    void spaceChanged(Space space);

    /// A handle was grabbed: snapshot the start transforms NOW (not at
    /// selection time -- panel edits between selection and drag would
    /// otherwise be reverted by the first gizmo move)
    void dragStarted();

    // Emitted during drag with current total delta
    void transformChanged(const glm::vec3& translation,
                          const glm::quat& rotation,
                          const glm::vec3& scale);

    // Emitted when drag commits
    void transformFinished();

    /// Emitted by cancelDrag(): restore the snapshots, keep no record
    void dragCancelled();

private:
    void resetDeltas();

    Mode m_mode = Mode::Translate;
    Space m_space = Space::World;

    bool m_isDragging = false;
    bool m_fineControl = false;

    glm::vec3 m_pivot{0.0f};

    // The grabbed handle and the frame it was drawn in (decomposed to keep
    // GizmoModel.hpp out of this header)
    int m_activeHandleValue = 0;  // editing::GizmoHandle
    glm::vec3 m_frameOrigin{0.0f};
    glm::mat3 m_frameAxes{1.0f};
    int m_dragAxis = -1;  // column of m_frameAxes, -1 for uniform scale

    // Press-time references, in the handle's own parameterization
    float m_refAxisT = 0.0f;         // axis handles: parameter along the line
    glm::vec3 m_refPlanePoint{0.0f}; // plane handles: intersection point
    float m_lastAngle = 0.0f;        // rings: last sample, for unwrapping
    float m_totalAngle = 0.0f;       // rings: unwrapped total
    float m_refDistance = 1.0f;      // uniform scale: closest-approach distance

    // Total deltas since press
    glm::vec3 m_deltaTranslation{0.0f};
    glm::quat m_deltaRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_deltaScale{1.0f};
};
