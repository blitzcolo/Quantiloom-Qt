#include "CameraMatrices.hpp"

#include <scene/Camera.hpp>

#include <cmath>

namespace vkview {

CameraMatrices CameraMatrices::fromCamera(const quantiloom::Camera& camera,
                                          float widthPx, float heightPx) {
    CameraMatrices m;
    m.m_position = camera.GetPosition();
    m.m_forward = camera.GetForward();
    m.m_right = camera.GetRight();
    m.m_up = camera.GetUp();
    m.m_fovScale = std::tan(glm::radians(camera.GetFovY()) * 0.5f);
    m.m_aspect = camera.GetAspectRatio();
    m.m_orthographic = camera.GetProjection() == quantiloom::Camera::Projection::Orthographic;
    m.m_orthoHeight = camera.GetOrthoHeight();
    m.m_width = widthPx > 0.0f ? widthPx : 1.0f;
    m.m_height = heightPx > 0.0f ? heightPx : 1.0f;
    return m;
}

glm::mat4 CameraMatrices::view() const {
    // Rows are the camera basis; RH view space looks down -z
    glm::mat4 v(1.0f);
    v[0][0] = m_right.x;    v[1][0] = m_right.y;    v[2][0] = m_right.z;
    v[0][1] = m_up.x;       v[1][1] = m_up.y;       v[2][1] = m_up.z;
    v[0][2] = -m_forward.x; v[1][2] = -m_forward.y; v[2][2] = -m_forward.z;
    v[3][0] = -glm::dot(m_right, m_position);
    v[3][1] = -glm::dot(m_up, m_position);
    v[3][2] = glm::dot(m_forward, m_position);
    return v;
}

glm::mat4 CameraMatrices::proj() const {
    // Zero-to-one depth, then the Vulkan y flip. Near/far only order the
    // overlay against itself (the scene is depth-tested in the grid shader
    // against the SDK's depth AOV, not against this projection).
    constexpr float kNear = 0.05f;
    constexpr float kFar = 200000.0f;

    if (m_orthographic) {
        // The overlay has to use the projection the SDK is rendering with, or
        // the grid and the gizmo sit where a perspective camera would have put
        // them and everything is subtly off the scene beneath it.
        const float halfH = m_orthoHeight * 0.5f;
        const float halfW = halfH * m_aspect;
        glm::mat4 p(1.0f);
        p[0][0] = 1.0f / halfW;
        p[1][1] = -1.0f / halfH;   // Vulkan: NDC y grows downward
        p[2][2] = 1.0f / (kNear - kFar);
        p[3][2] = kNear / (kNear - kFar);
        return p;
    }

    const float f = 1.0f / m_fovScale;

    glm::mat4 p(0.0f);
    p[0][0] = f / m_aspect;
    p[1][1] = -f;  // Vulkan: NDC y grows downward
    p[2][2] = kFar / (kNear - kFar);
    p[2][3] = -1.0f;
    p[3][2] = (kNear * kFar) / (kNear - kFar);
    return p;
}

CameraRay CameraMatrices::rayThroughPixel(float px, float py) const {
    // Byte-for-byte the raygen.rgen mapping: pixel center, NDC y flipped
    const float u = (px + 0.5f) / m_width;
    const float v = (py + 0.5f) / m_height;
    glm::vec2 ndc(u * 2.0f - 1.0f, -(v * 2.0f - 1.0f));

    CameraRay ray;
    if (m_orthographic) {
        // Byte-for-byte raygen's orthographic branch, for the same reason the
        // pick shader mirrors it: a gizmo hit test that used a perspective ray
        // would grab a handle the user is not pointing at.
        const float halfH = m_orthoHeight * 0.5f;
        const float halfW = halfH * m_aspect;
        ray.origin = m_position + ndc.x * m_right * halfW + ndc.y * m_up * halfH;
        ray.direction = glm::normalize(m_forward);
        return ray;
    }
    ray.origin = m_position;
    ray.direction = glm::normalize(m_forward +
                                   ndc.x * m_right * m_fovScale * m_aspect +
                                   ndc.y * m_up * m_fovScale);
    return ray;
}

}  // namespace vkview
