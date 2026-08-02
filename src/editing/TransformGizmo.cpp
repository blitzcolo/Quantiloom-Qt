/**
 * @file TransformGizmo.cpp
 * @brief Ray-based drag math for the viewport transform gizmo
 */

#include "TransformGizmo.hpp"
#include "GizmoModel.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace {

// Snap increments (Ctrl); fine (Shift) tightens them
constexpr float kSnapTranslate = 0.5f;
constexpr float kSnapTranslateFine = 0.05f;
constexpr float kSnapRotateDeg = 15.0f;
constexpr float kSnapRotateDegFine = 5.0f;
constexpr float kSnapScale = 0.1f;

float snapTo(float value, float step) {
    return std::round(value / step) * step;
}

/// Closest-approach parameters between a ray and an infinite line.
/// Returns false when they are near parallel (the drag freezes rather than
/// shooting the object to infinity).
bool rayLineClosest(const vkview::CameraRay& ray, const glm::vec3& lineOrigin,
                    const glm::vec3& lineDir, float& tLine) {
    const glm::vec3 w0 = ray.origin - lineOrigin;
    const float b = glm::dot(ray.direction, lineDir);
    const float d = glm::dot(ray.direction, w0);
    const float e = glm::dot(lineDir, w0);
    const float denom = 1.0f - b * b;
    if (std::abs(denom) < 1e-4f) {
        return false;
    }
    tLine = (e - b * d) / denom;
    return true;
}

bool rayPlane(const vkview::CameraRay& ray, const glm::vec3& planeOrigin,
              const glm::vec3& normal, glm::vec3& point) {
    const float denom = glm::dot(ray.direction, normal);
    if (std::abs(denom) < 1e-6f) {
        return false;
    }
    const float t = glm::dot(planeOrigin - ray.origin, normal) / denom;
    if (t <= 0.0f) {
        return false;
    }
    point = ray.at(t);
    return true;
}

/// Angle of `p` (relative to origin) inside the plane spanned by u, v
float angleInPlane(const glm::vec3& p, const glm::vec3& u, const glm::vec3& v) {
    return std::atan2(glm::dot(p, v), glm::dot(p, u));
}

/// Basis of the ring plane for axis `n`: matches GizmoModel's axisBasis so
/// the reference angle lands where the ring was drawn
void ringBasis(const glm::vec3& n, glm::vec3& u, glm::vec3& v) {
    const glm::vec3 helper = std::abs(n.y) < 0.99f ? glm::vec3(0, 1, 0)
                                                   : glm::vec3(1, 0, 0);
    u = glm::normalize(glm::cross(n, helper));
    v = glm::cross(n, u);
}

float shortestAngleDiff(float from, float to) {
    float diff = to - from;
    while (diff > glm::pi<float>()) diff -= glm::two_pi<float>();
    while (diff < -glm::pi<float>()) diff += glm::two_pi<float>();
    return diff;
}

}  // namespace

TransformGizmo::TransformGizmo(QObject* parent)
    : QObject(parent)
{
}

void TransformGizmo::setMode(Mode mode) {
    if (m_mode != mode) {
        m_mode = mode;
        emit modeChanged(mode);
    }
}

void TransformGizmo::setSpace(Space space) {
    if (m_space != space) {
        m_space = space;
        emit spaceChanged(space);
    }
}

void TransformGizmo::toggleSpace() {
    setSpace(m_space == Space::World ? Space::Local : Space::World);
}

editing::GizmoHandle TransformGizmo::activeHandle() const {
    return m_isDragging ? static_cast<editing::GizmoHandle>(m_activeHandleValue)
                        : editing::GizmoHandle::None;
}

void TransformGizmo::resetDeltas() {
    m_deltaTranslation = glm::vec3(0.0f);
    m_deltaRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    m_deltaScale = glm::vec3(1.0f);
}

void TransformGizmo::beginHandleDrag(editing::GizmoHandle handle,
                                     const vkview::CameraRay& pressRay,
                                     const editing::GizmoFrame& frame) {
    m_activeHandleValue = static_cast<int>(handle);
    m_frameOrigin = frame.origin;
    m_frameAxes = frame.axes;
    m_pivot = frame.origin;
    m_dragAxis = editing::gizmoHandleAxis(handle);
    resetDeltas();

    using editing::GizmoHandle;
    bool referenceOk = false;
    switch (handle) {
        case GizmoHandle::AxisX:
        case GizmoHandle::AxisY:
        case GizmoHandle::AxisZ:
        case GizmoHandle::ScaleX:
        case GizmoHandle::ScaleY:
        case GizmoHandle::ScaleZ:
            referenceOk = rayLineClosest(pressRay, m_frameOrigin,
                                         m_frameAxes[m_dragAxis], m_refAxisT);
            // A reference too close to the origin makes scale ratios explode
            if (referenceOk && (handle == GizmoHandle::ScaleX ||
                                handle == GizmoHandle::ScaleY ||
                                handle == GizmoHandle::ScaleZ)) {
                referenceOk = std::abs(m_refAxisT) > 1e-4f * frame.scale;
            }
            break;
        case GizmoHandle::PlaneXY:
        case GizmoHandle::PlaneXZ:
        case GizmoHandle::PlaneYZ:
            referenceOk = rayPlane(pressRay, m_frameOrigin,
                                   m_frameAxes[m_dragAxis], m_refPlanePoint);
            break;
        case GizmoHandle::RingX:
        case GizmoHandle::RingY:
        case GizmoHandle::RingZ: {
            glm::vec3 point;
            referenceOk = rayPlane(pressRay, m_frameOrigin,
                                   m_frameAxes[m_dragAxis], point);
            if (referenceOk) {
                glm::vec3 u, v;
                ringBasis(m_frameAxes[m_dragAxis], u, v);
                m_lastAngle = angleInPlane(point - m_frameOrigin, u, v);
                m_totalAngle = 0.0f;
            }
            break;
        }
        case GizmoHandle::ScaleUniform: {
            const float tRay =
                glm::dot(m_frameOrigin - pressRay.origin, pressRay.direction);
            m_refDistance = glm::length(pressRay.at(tRay) - m_frameOrigin);
            referenceOk = m_refDistance > 1e-4f * frame.scale;
            break;
        }
        default:
            break;
    }

    if (!referenceOk) {
        m_activeHandleValue = static_cast<int>(GizmoHandle::None);
        return;
    }

    m_isDragging = true;
    emit dragStarted();
}

void TransformGizmo::updateHandleDrag(const vkview::CameraRay& ray, bool snap) {
    if (!m_isDragging) {
        return;
    }

    const float fine = m_fineControl ? 0.1f : 1.0f;

    using editing::GizmoHandle;
    switch (static_cast<GizmoHandle>(m_activeHandleValue)) {
        case GizmoHandle::AxisX:
        case GizmoHandle::AxisY:
        case GizmoHandle::AxisZ: {
            float t = 0.0f;
            if (!rayLineClosest(ray, m_frameOrigin, m_frameAxes[m_dragAxis], t)) {
                return;  // near parallel: freeze rather than jump
            }
            float delta = (t - m_refAxisT) * fine;
            if (snap) {
                delta = snapTo(delta, m_fineControl ? kSnapTranslateFine
                                                    : kSnapTranslate);
            }
            m_deltaTranslation = m_frameAxes[m_dragAxis] * delta;
            break;
        }
        case GizmoHandle::PlaneXY:
        case GizmoHandle::PlaneXZ:
        case GizmoHandle::PlaneYZ: {
            glm::vec3 point;
            if (!rayPlane(ray, m_frameOrigin, m_frameAxes[m_dragAxis], point)) {
                return;
            }
            const int a = (m_dragAxis + 1) % 3;
            const int b = (m_dragAxis + 2) % 3;
            const glm::vec3 d = point - m_refPlanePoint;
            float da = glm::dot(d, m_frameAxes[a]) * fine;
            float db = glm::dot(d, m_frameAxes[b]) * fine;
            if (snap) {
                const float step = m_fineControl ? kSnapTranslateFine : kSnapTranslate;
                da = snapTo(da, step);
                db = snapTo(db, step);
            }
            m_deltaTranslation = m_frameAxes[a] * da + m_frameAxes[b] * db;
            break;
        }
        case GizmoHandle::RingX:
        case GizmoHandle::RingY:
        case GizmoHandle::RingZ: {
            glm::vec3 point;
            if (!rayPlane(ray, m_frameOrigin, m_frameAxes[m_dragAxis], point)) {
                return;
            }
            glm::vec3 u, v;
            ringBasis(m_frameAxes[m_dragAxis], u, v);
            const float angle = angleInPlane(point - m_frameOrigin, u, v);
            // Unwrap so a drag can wind past 180 degrees and keep going
            m_totalAngle += shortestAngleDiff(m_lastAngle, angle) * fine;
            m_lastAngle = angle;

            float applied = m_totalAngle;
            if (snap) {
                const float step = glm::radians(m_fineControl ? kSnapRotateDegFine
                                                              : kSnapRotateDeg);
                applied = snapTo(applied, step);
            }
            m_deltaRotation = glm::angleAxis(applied, m_frameAxes[m_dragAxis]);
            break;
        }
        case GizmoHandle::ScaleX:
        case GizmoHandle::ScaleY:
        case GizmoHandle::ScaleZ: {
            float t = 0.0f;
            if (!rayLineClosest(ray, m_frameOrigin, m_frameAxes[m_dragAxis], t)) {
                return;
            }
            float factor = 1.0f + (t / m_refAxisT - 1.0f) * fine;
            if (snap) {
                factor = snapTo(factor, kSnapScale);
            }
            factor = std::max(factor, 0.01f);
            m_deltaScale = glm::vec3(1.0f);
            m_deltaScale[m_dragAxis] = factor;
            break;
        }
        case GizmoHandle::ScaleUniform: {
            const float tRay = glm::dot(m_frameOrigin - ray.origin, ray.direction);
            const float distance = glm::length(ray.at(tRay) - m_frameOrigin);
            float factor = 1.0f + (distance / m_refDistance - 1.0f) * fine;
            if (snap) {
                factor = snapTo(factor, kSnapScale);
            }
            factor = std::max(factor, 0.01f);
            m_deltaScale = glm::vec3(factor);
            break;
        }
        default:
            return;
    }

    emit transformChanged(m_deltaTranslation, m_deltaRotation, m_deltaScale);
}

void TransformGizmo::endDrag() {
    if (m_isDragging) {
        m_isDragging = false;
        m_activeHandleValue = static_cast<int>(editing::GizmoHandle::None);
        emit transformFinished();
    }
}

void TransformGizmo::cancelDrag() {
    if (m_isDragging) {
        m_isDragging = false;
        m_activeHandleValue = static_cast<int>(editing::GizmoHandle::None);
        resetDeltas();
        emit dragCancelled();
    }
}

glm::mat4 TransformGizmo::applyDelta(const glm::mat4& original) const {
    switch (m_mode) {
        case Mode::Translate: {
            // The delta was built from the frame's axes, so world and local
            // space are both plain world-space translations here
            glm::mat4 result = original;
            result[3] = glm::vec4(glm::vec3(original[3]) + m_deltaTranslation, 1.0f);
            return result;
        }

        case Mode::Rotate: {
            // World: about the selection center. Local: about the node's own
            // origin (the quat axis already points along the local axis).
            const glm::vec3 pivot =
                m_space == Space::World ? m_pivot : glm::vec3(original[3]);
            const glm::mat4 rotation = glm::toMat4(m_deltaRotation);
            return glm::translate(glm::mat4(1.0f), pivot) * rotation *
                   glm::translate(glm::mat4(1.0f), -pivot) * original;
        }

        case Mode::Scale: {
            // Scale along the frame's axes: A * S * A^T is the scale in the
            // frame's space (A orthonormal), so world mode scales along world
            // axes and local mode along the node's own
            const glm::vec3 pivot =
                m_space == Space::World ? m_pivot : glm::vec3(original[3]);
            const glm::mat4 axes(m_frameAxes);
            const glm::mat4 scaleInFrame =
                axes * glm::scale(glm::mat4(1.0f), m_deltaScale) * glm::transpose(axes);
            return glm::translate(glm::mat4(1.0f), pivot) * scaleInFrame *
                   glm::translate(glm::mat4(1.0f), -pivot) * original;
        }
    }
    return original;
}
