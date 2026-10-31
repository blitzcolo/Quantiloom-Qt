/**
 * @file CameraMatrices.hpp
 * @brief View/projection matrices and pixel rays from the SDK's resolved camera
 *
 * The single source of truth for everything overlay- and picking-related that
 * needs camera geometry on the Qt side. Built from ExternalRenderContext::
 * GetCamera() -- the exact basis the raygen shader uses -- never from this
 * repo's own orbit state, so a ray or a projected vertex agrees with the
 * rendered frame pixel-for-pixel.
 *
 * Conventions, matching raygen.rgen:
 *  - fovScale = tan(fovY / 2), aspect multiplies the x term separately
 *  - pixel y grows downward while camera up points up, so a pixel's NDC y is
 *    flipped (rayThroughPixel)
 *  - ray directions are normalized, so a hit distance is Euclidean
 *
 * The projection is OpenGL's convention (y up, depth -1..1); a backend's
 * own comes from QRhi::clipSpaceCorrMatrix() in front of it, which on Vulkan
 * is the y flip and the 0..1 depth this used to hard-code.
 *
 * @author blitzcolo
 */

#pragma once

#include <glm/glm.hpp>

namespace quantiloom {
class Camera;
}

namespace viewport {

struct CameraRay {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};

    [[nodiscard]] glm::vec3 at(float t) const { return origin + direction * t; }
};

class CameraMatrices {
public:
    CameraMatrices() = default;

    /// From the SDK's resolved camera plus the viewport size in device pixels.
    /// The camera's own aspect is used (it is what raygen renders with); the
    /// size is only needed to turn pixel coordinates into NDC.
    static CameraMatrices fromCamera(const quantiloom::Camera& camera,
                                     float widthPx, float heightPx);

    /// Right-handed view matrix straight from the basis vectors -- nothing is
    /// recomputed, so it cannot drift from what the SDK resolved.
    [[nodiscard]] glm::mat4 view() const;

    /// OpenGL convention (y up, depth -1..1), perspective or orthographic to
    /// match the camera it came from. Draw with
    /// QRhi::clipSpaceCorrMatrix() * projGL() * view().
    [[nodiscard]] glm::mat4 projGL() const;

    /// The raygen shader's primary ray through a pixel (pixel center, no
    /// jitter). Input in the same device-pixel space the SDK renders at.
    [[nodiscard]] CameraRay rayThroughPixel(float px, float py) const;

    [[nodiscard]] const glm::vec3& position() const { return m_position; }
    [[nodiscard]] const glm::vec3& forward() const { return m_forward; }
    [[nodiscard]] const glm::vec3& right() const { return m_right; }
    [[nodiscard]] const glm::vec3& up() const { return m_up; }
    [[nodiscard]] float fovScale() const { return m_fovScale; }
    [[nodiscard]] float aspect() const { return m_aspect; }
    [[nodiscard]] bool orthographic() const { return m_orthographic; }
    [[nodiscard]] float orthoHeight() const { return m_orthoHeight; }

private:
    glm::vec3 m_position{0.0f};
    glm::vec3 m_forward{0.0f, 0.0f, -1.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    float m_fovScale = 0.4142f;  // tan(45deg / 2)
    float m_aspect = 1.0f;
    float m_width = 1.0f;
    float m_height = 1.0f;
    /// Mirrors the SDK camera, so the overlay draws with the projection the
    /// scene beneath it was rendered with.
    bool m_orthographic = false;
    float m_orthoHeight = 2.0f;
};

}  // namespace viewport
