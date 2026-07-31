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
    ray.origin = m_position;
    ray.direction = glm::normalize(m_forward +
                                   ndc.x * m_right * m_fovScale * m_aspect +
                                   ndc.y * m_up * m_fovScale);
    return ray;
}

}  // namespace vkview
