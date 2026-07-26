/**
 * @file TransformGizmo.cpp
 * @brief Transform gizmo implementation
 */

#include "TransformGizmo.hpp"

#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <QDebug>

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

void TransformGizmo::setAxisConstraint(Axis axis) {
    if (m_axisConstraint == axis) {
        return;
    }
    m_axisConstraint = axis;
    emit axisConstraintChanged(m_axisConstraint);
}

void TransformGizmo::toggleAxisConstraint(Axis axis) {
    // Toggling the active constraint off goes back to all axes.
    setAxisConstraint(m_axisConstraint == axis ? Axis::XYZ : axis);
}

void TransformGizmo::beginDrag(const QPointF& screenPos,
                                const glm::vec3& cameraPos,
                                const glm::vec3& cameraForward,
                                const glm::vec3& cameraRight,
                                const glm::vec3& cameraUp) {
    m_isDragging = true;
    m_dragStart = screenPos;
    m_lastDragPos = screenPos;

    m_cameraPos = cameraPos;
    m_cameraForward = cameraForward;
    m_cameraRight = cameraRight;
    m_cameraUp = cameraUp;

    // Reset deltas
    m_deltaTranslation = glm::vec3(0.0f);
    m_deltaRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    m_deltaScale = glm::vec3(1.0f);
}

void TransformGizmo::updateDrag(const QPointF& screenPos) {
    if (!m_isDragging) {
        return;
    }

    QPointF screenDelta = screenPos - m_lastDragPos;
    m_lastDragPos = screenPos;

    qDebug() << "TransformGizmo::updateDrag - screenDelta:" << screenDelta.x() << screenDelta.y();

    // Apply fine control
    float multiplier = m_fineControl ? 0.1f : 1.0f;

    switch (m_mode) {
        case Mode::Translate: {
            glm::vec3 worldDelta = screenToWorldDelta(screenDelta);
            worldDelta = applyAxisConstraint(worldDelta);
            worldDelta *= m_translateSensitivity * multiplier;
            m_deltaTranslation += worldDelta;
            break;
        }

        case Mode::Rotate: {
            // Use screen X for rotation around view-aligned axis
            // Use screen Y for rotation around perpendicular axis
            float angleX = static_cast<float>(screenDelta.x()) * m_rotateSensitivity * multiplier;
            float angleY = static_cast<float>(screenDelta.y()) * m_rotateSensitivity * multiplier;

            glm::quat rotDelta(1.0f, 0.0f, 0.0f, 0.0f);

            if (m_axisConstraint == Axis::X || m_axisConstraint == Axis::XYZ) {
                rotDelta = glm::angleAxis(glm::radians(angleY), glm::vec3(1, 0, 0)) * rotDelta;
            }
            if (m_axisConstraint == Axis::Y || m_axisConstraint == Axis::XYZ) {
                rotDelta = glm::angleAxis(glm::radians(-angleX), glm::vec3(0, 1, 0)) * rotDelta;
            }
            if (m_axisConstraint == Axis::Z) {
                // For Z axis, use combined screen movement
                float angle = static_cast<float>(screenDelta.x() + screenDelta.y())
                              * m_rotateSensitivity * multiplier * 0.5f;
                rotDelta = glm::angleAxis(glm::radians(angle), glm::vec3(0, 0, 1));
            }

            m_deltaRotation = rotDelta * m_deltaRotation;
            break;
        }

        case Mode::Scale: {
            // Uniform scale by default, axis-constrained if specified
            float scaleFactor = 1.0f + static_cast<float>(screenDelta.x() + screenDelta.y())
                                * m_scaleSensitivity * multiplier;

            // Clamp scale factor to prevent zero/negative scale
            scaleFactor = std::max(scaleFactor, 0.01f);

            glm::vec3 scaleVec(scaleFactor, scaleFactor, scaleFactor);

            if (m_axisConstraint != Axis::XYZ) {
                // Non-uniform scale
                scaleVec = glm::vec3(1.0f);
                if (m_axisConstraint & Axis::X) scaleVec.x = scaleFactor;
                if (m_axisConstraint & Axis::Y) scaleVec.y = scaleFactor;
                if (m_axisConstraint & Axis::Z) scaleVec.z = scaleFactor;
            }

            m_deltaScale *= scaleVec;
            break;
        }
    }

    emit transformChanged(m_deltaTranslation, m_deltaRotation, m_deltaScale);
}

void TransformGizmo::endDrag() {
    if (m_isDragging) {
        m_isDragging = false;
        emit transformFinished();
    }
}

void TransformGizmo::setInitialTransform(const glm::mat4& transform) {
    m_initialTransform = transform;
}

glm::mat4 TransformGizmo::applyDelta(const glm::mat4& original) const {
    // Decompose original transform around pivot
    glm::mat4 result = original;

    // Extract original translation
    glm::vec3 originalPos(original[3]);

    switch (m_mode) {
        case Mode::Translate:
            result[3] = glm::vec4(originalPos + m_deltaTranslation, 1.0f);
            break;

        case Mode::Rotate: {
            // Rotate around pivot point
            glm::mat4 toPivot = glm::translate(glm::mat4(1.0f), -m_pivot);
            glm::mat4 fromPivot = glm::translate(glm::mat4(1.0f), m_pivot);
            glm::mat4 rot = glm::toMat4(m_deltaRotation);

            if (m_space == Space::World) {
                result = fromPivot * rot * toPivot * original;
            } else {
                // Local space rotation
                glm::mat3 localAxes(original);
                glm::quat localRot = m_deltaRotation;
                result = original * glm::toMat4(localRot);
            }
            break;
        }

        case Mode::Scale: {
            // Scale around pivot point
            glm::mat4 toPivot = glm::translate(glm::mat4(1.0f), -m_pivot);
            glm::mat4 fromPivot = glm::translate(glm::mat4(1.0f), m_pivot);
            glm::mat4 scale = glm::scale(glm::mat4(1.0f), m_deltaScale);

            if (m_space == Space::World) {
                result = fromPivot * scale * toPivot * original;
            } else {
                // Local space scale - scale along local axes
                // Extract rotation, apply scale, recompose
                glm::mat3 rotation(original);
                glm::vec3 currentScale(
                    glm::length(original[0]),
                    glm::length(original[1]),
                    glm::length(original[2])
                );

                glm::vec3 newScale = currentScale * m_deltaScale;

                result = glm::mat4(1.0f);
                result[0] = glm::vec4(glm::normalize(glm::vec3(original[0])) * newScale.x, 0.0f);
                result[1] = glm::vec4(glm::normalize(glm::vec3(original[1])) * newScale.y, 0.0f);
                result[2] = glm::vec4(glm::normalize(glm::vec3(original[2])) * newScale.z, 0.0f);
                result[3] = original[3];
            }
            break;
        }
    }

    return result;
}

glm::vec3 TransformGizmo::applyAxisConstraint(const glm::vec3& delta) const {
    if (m_axisConstraint == Axis::XYZ) {
        return delta;
    }

    glm::vec3 result(0.0f);
    if (m_axisConstraint & Axis::X) result.x = delta.x;
    if (m_axisConstraint & Axis::Y) result.y = delta.y;
    if (m_axisConstraint & Axis::Z) result.z = delta.z;
    return result;
}

glm::vec3 TransformGizmo::screenToWorldDelta(const QPointF& screenDelta) const {
    // Convert screen delta to world space delta using camera orientation
    // Horizontal screen movement -> camera right direction
    // Vertical screen movement -> camera up direction

    glm::vec3 worldDelta(0.0f);
    worldDelta += m_cameraRight * static_cast<float>(screenDelta.x());
    worldDelta -= m_cameraUp * static_cast<float>(screenDelta.y());

    return worldDelta;
}
