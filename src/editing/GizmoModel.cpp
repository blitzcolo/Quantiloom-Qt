#include "GizmoModel.hpp"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace editing {

namespace {

// Handle set dimensions, in gizmo units (scaled by GizmoFrame::scale)
constexpr float kShaftRadius = 0.02f;
constexpr float kShaftLength = 0.75f;
constexpr float kConeRadius = 0.07f;
constexpr float kConeLength = 0.25f;
constexpr float kPlaneOffset = 0.35f;
constexpr float kPlaneSize = 0.25f;
constexpr float kRingRadius = 1.0f;
constexpr float kRingHalfWidth = 0.03f;
constexpr float kScaleCube = 0.10f;
constexpr float kUniformCube = 0.12f;

// Pick thresholds, same units, so the grab radius is screen-constant
constexpr float kAxisPickRadius = 0.10f;
constexpr float kRingPickBand = 0.09f;

// Axis colors match the grid's palette; hover yellow, active white.
// LINEAR values (the sRGB swapchain encodes on write): each is the linear
// form of the intended display color -- X #DB4F54, Y #8CC740, Z #4A6BDB
// -- writing display values directly would come out washed out.
const glm::vec4 kAxisColors[3] = {
    {0.718f, 0.076f, 0.088f, 1.0f},  // X
    {0.268f, 0.579f, 0.048f, 1.0f},  // Y
    {0.066f, 0.148f, 0.718f, 1.0f},  // Z
};
const glm::vec4 kHoverColor{1.0f, 0.699f, 0.029f, 1.0f};
const glm::vec4 kActiveColor{1.0f, 1.0f, 1.0f, 1.0f};
const glm::vec4 kUniformColor{0.531f, 0.531f, 0.531f, 1.0f};

glm::vec4 handleColor(GizmoHandle handle, GizmoHandle hovered, GizmoHandle active,
                      const glm::vec4& base) {
    if (handle == active) return kActiveColor;
    if (handle == hovered) return kHoverColor;
    return base;
}

/// Two vectors orthogonal to `axis`, for sweeping circles around it
void axisBasis(const glm::vec3& axis, glm::vec3& u, glm::vec3& v) {
    const glm::vec3 helper = std::abs(axis.y) < 0.99f ? glm::vec3(0, 1, 0)
                                                      : glm::vec3(1, 0, 0);
    u = glm::normalize(glm::cross(axis, helper));
    v = glm::cross(axis, u);
}

void appendTri(std::vector<GizmoVertex>& out, const glm::vec3& a, const glm::vec3& b,
               const glm::vec3& c, const glm::vec4& color) {
    out.push_back({a, color});
    out.push_back({b, color});
    out.push_back({c, color});
}

void appendQuad(std::vector<GizmoVertex>& out, const glm::vec3& a, const glm::vec3& b,
                const glm::vec3& c, const glm::vec3& d, const glm::vec4& color) {
    appendTri(out, a, b, c, color);
    appendTri(out, a, c, d, color);
}

void appendCylinder(std::vector<GizmoVertex>& out, const glm::vec3& from,
                    const glm::vec3& to, float radius, const glm::vec4& color,
                    int sides = 8) {
    const glm::vec3 axis = glm::normalize(to - from);
    glm::vec3 u, v;
    axisBasis(axis, u, v);
    for (int i = 0; i < sides; ++i) {
        const float a0 = glm::two_pi<float>() * float(i) / float(sides);
        const float a1 = glm::two_pi<float>() * float(i + 1) / float(sides);
        const glm::vec3 r0 = (u * std::cos(a0) + v * std::sin(a0)) * radius;
        const glm::vec3 r1 = (u * std::cos(a1) + v * std::sin(a1)) * radius;
        appendQuad(out, from + r0, from + r1, to + r1, to + r0, color);
    }
}

void appendCone(std::vector<GizmoVertex>& out, const glm::vec3& base,
                const glm::vec3& tip, float radius, const glm::vec4& color,
                int sides = 12) {
    const glm::vec3 axis = glm::normalize(tip - base);
    glm::vec3 u, v;
    axisBasis(axis, u, v);
    for (int i = 0; i < sides; ++i) {
        const float a0 = glm::two_pi<float>() * float(i) / float(sides);
        const float a1 = glm::two_pi<float>() * float(i + 1) / float(sides);
        const glm::vec3 r0 = base + (u * std::cos(a0) + v * std::sin(a0)) * radius;
        const glm::vec3 r1 = base + (u * std::cos(a1) + v * std::sin(a1)) * radius;
        appendTri(out, r0, r1, tip, color);
        appendTri(out, r1, r0, base, color);  // cap
    }
}

void appendCube(std::vector<GizmoVertex>& out, const GizmoFrame& frame,
                const glm::vec3& centerLocal, float halfLocal, const glm::vec4& color) {
    // Corners in gizmo space, transformed through the frame
    glm::vec3 c[8];
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 offset((i & 1) ? halfLocal : -halfLocal,
                               (i & 2) ? halfLocal : -halfLocal,
                               (i & 4) ? halfLocal : -halfLocal);
        c[i] = frame.origin + frame.axes * ((centerLocal + offset) * frame.scale);
    }
    appendQuad(out, c[0], c[1], c[3], c[2], color);  // -z
    appendQuad(out, c[4], c[6], c[7], c[5], color);  // +z
    appendQuad(out, c[0], c[4], c[5], c[1], color);  // -y
    appendQuad(out, c[2], c[3], c[7], c[6], color);  // +y
    appendQuad(out, c[0], c[2], c[6], c[4], color);  // -x
    appendQuad(out, c[1], c[5], c[7], c[3], color);  // +x
}

void appendRing(std::vector<GizmoVertex>& out, const GizmoFrame& frame, int axisIndex,
                const glm::vec4& color, const glm::vec3& viewDir, int segments = 48) {
    const glm::vec3 normal = frame.axes[axisIndex];
    glm::vec3 u, v;
    axisBasis(normal, u, v);
    const float rInner = (kRingRadius - kRingHalfWidth) * frame.scale;
    const float rOuter = (kRingRadius + kRingHalfWidth) * frame.scale;
    for (int i = 0; i < segments; ++i) {
        const float a0 = glm::two_pi<float>() * float(i) / float(segments);
        const float a1 = glm::two_pi<float>() * float(i + 1) / float(segments);
        const glm::vec3 d0 = u * std::cos(a0) + v * std::sin(a0);
        const glm::vec3 d1 = u * std::cos(a1) + v * std::sin(a1);
        // The half of the ring facing away from the camera reads at 30%,
        // Blender-style, so front and back are never confused
        glm::vec4 c = color;
        if (glm::dot(d0 + d1, -viewDir) < 0.0f) {
            c.a *= 0.3f;
        }
        appendQuad(out,
                   frame.origin + d0 * rInner, frame.origin + d0 * rOuter,
                   frame.origin + d1 * rOuter, frame.origin + d1 * rInner, c);
    }
}

/// Closest-approach parameters between a ray and an infinite line.
/// Returns false when they are near parallel.
bool rayLineClosest(const vkview::CameraRay& ray, const glm::vec3& lineOrigin,
                    const glm::vec3& lineDir, float& tRay, float& tLine) {
    const glm::vec3 w0 = ray.origin - lineOrigin;
    const float b = glm::dot(ray.direction, lineDir);
    const float d = glm::dot(ray.direction, w0);
    const float e = glm::dot(lineDir, w0);
    const float denom = 1.0f - b * b;
    if (std::abs(denom) < 1e-6f) {
        return false;
    }
    tRay = (b * e - d) / denom;
    tLine = (e - b * d) / denom;
    return true;
}

bool rayPlane(const vkview::CameraRay& ray, const glm::vec3& planeOrigin,
              const glm::vec3& normal, float& t) {
    const float denom = glm::dot(ray.direction, normal);
    if (std::abs(denom) < 1e-7f) {
        return false;
    }
    t = glm::dot(planeOrigin - ray.origin, normal) / denom;
    return t > 0.0f;
}

struct HitCandidate {
    GizmoHandle handle = GizmoHandle::None;
    int priority = 0;  // higher wins: planes 3, axes/cubes 2, rings 1
    float t = 1e30f;
};

void consider(HitCandidate& best, GizmoHandle handle, int priority, float t) {
    if (priority > best.priority ||
        (priority == best.priority && t < best.t)) {
        best = {handle, priority, t};
    }
}

void hitAxisShaft(HitCandidate& best, const vkview::CameraRay& ray,
                  const GizmoFrame& frame, int axisIndex, GizmoHandle handle,
                  float maxParam) {
    const glm::vec3 dir = frame.axes[axisIndex];
    float tRay = 0.0f;
    float tLine = 0.0f;
    if (!rayLineClosest(ray, frame.origin, dir, tRay, tLine)) {
        return;
    }
    if (tRay <= 0.0f) {
        return;
    }
    const float param = tLine / frame.scale;  // in gizmo units along the axis
    if (param < 0.05f || param > maxParam) {
        return;
    }
    const glm::vec3 onRay = ray.at(tRay);
    const glm::vec3 onLine = frame.origin + dir * tLine;
    if (glm::length(onRay - onLine) <= kAxisPickRadius * frame.scale) {
        consider(best, handle, 2, tRay);
    }
}

}  // namespace

int gizmoHandleAxis(GizmoHandle handle) {
    switch (handle) {
        case GizmoHandle::AxisX:
        case GizmoHandle::RingX:
        case GizmoHandle::ScaleX:
        case GizmoHandle::PlaneYZ:
            return 0;
        case GizmoHandle::AxisY:
        case GizmoHandle::RingY:
        case GizmoHandle::ScaleY:
        case GizmoHandle::PlaneXZ:
            return 1;
        case GizmoHandle::AxisZ:
        case GizmoHandle::RingZ:
        case GizmoHandle::ScaleZ:
        case GizmoHandle::PlaneXY:
            return 2;
        default:
            return -1;
    }
}

float GizmoFrame::screenScale(const glm::vec3& cameraPos, const glm::vec3& origin,
                              float fovScale) {
    const float distance = std::max(glm::length(origin - cameraPos), 1e-4f);
    // ~11% of the viewport height regardless of distance or scene scale
    return 0.22f * distance * fovScale;
}

void buildGizmoGeometry(TransformGizmo::Mode mode, const GizmoFrame& frame,
                        GizmoHandle hovered, GizmoHandle active,
                        const glm::vec3& viewDir,
                        std::vector<GizmoVertex>& out) {
    const auto axisPoint = [&](int axis, float param) {
        return frame.origin + frame.axes[axis] * (param * frame.scale);
    };

    switch (mode) {
        case TransformGizmo::Mode::Translate: {
            static constexpr GizmoHandle axisHandles[3] = {
                GizmoHandle::AxisX, GizmoHandle::AxisY, GizmoHandle::AxisZ};
            for (int i = 0; i < 3; ++i) {
                const glm::vec4 c =
                    handleColor(axisHandles[i], hovered, active, kAxisColors[i]);
                appendCylinder(out, axisPoint(i, 0.05f), axisPoint(i, kShaftLength),
                               kShaftRadius * frame.scale, c);
                appendCone(out, axisPoint(i, kShaftLength),
                           axisPoint(i, kShaftLength + kConeLength),
                           kConeRadius * frame.scale, c);
            }
            // Plane quads sit between their two axes; fill translucent, so
            // the object under them stays readable
            static constexpr GizmoHandle planeHandles[3] = {
                GizmoHandle::PlaneYZ, GizmoHandle::PlaneXZ, GizmoHandle::PlaneXY};
            for (int i = 0; i < 3; ++i) {
                const int a = (i + 1) % 3;
                const int b = (i + 2) % 3;
                glm::vec4 c = handleColor(planeHandles[i], hovered, active,
                                          kAxisColors[i]);
                if (planeHandles[i] != hovered && planeHandles[i] != active) {
                    c.a = 0.4f;
                }
                const glm::vec3 pa = frame.axes[a] * (kPlaneOffset * frame.scale);
                const glm::vec3 pb = frame.axes[b] * (kPlaneOffset * frame.scale);
                const glm::vec3 sa = frame.axes[a] * (kPlaneSize * frame.scale);
                const glm::vec3 sb = frame.axes[b] * (kPlaneSize * frame.scale);
                const glm::vec3 corner = frame.origin + pa + pb;
                appendQuad(out, corner, corner + sa, corner + sa + sb, corner + sb, c);
            }
            break;
        }
        case TransformGizmo::Mode::Rotate: {
            static constexpr GizmoHandle ringHandles[3] = {
                GizmoHandle::RingX, GizmoHandle::RingY, GizmoHandle::RingZ};
            for (int i = 0; i < 3; ++i) {
                const glm::vec4 c =
                    handleColor(ringHandles[i], hovered, active, kAxisColors[i]);
                appendRing(out, frame, i, c, viewDir);
            }
            break;
        }
        case TransformGizmo::Mode::Scale: {
            static constexpr GizmoHandle scaleHandles[3] = {
                GizmoHandle::ScaleX, GizmoHandle::ScaleY, GizmoHandle::ScaleZ};
            for (int i = 0; i < 3; ++i) {
                const glm::vec4 c =
                    handleColor(scaleHandles[i], hovered, active, kAxisColors[i]);
                appendCylinder(out, axisPoint(i, 0.05f), axisPoint(i, kShaftLength),
                               kShaftRadius * frame.scale, c);
                glm::vec3 centerLocal(0.0f);
                centerLocal[i] = kShaftLength + kScaleCube;
                appendCube(out, frame, centerLocal, kScaleCube, c);
            }
            appendCube(out, frame, glm::vec3(0.0f), kUniformCube,
                       handleColor(GizmoHandle::ScaleUniform, hovered, active,
                                   kUniformColor));
            break;
        }
    }
}

GizmoHandle hitTestGizmo(const vkview::CameraRay& ray, const GizmoFrame& frame,
                         TransformGizmo::Mode mode) {
    HitCandidate best;

    switch (mode) {
        case TransformGizmo::Mode::Translate: {
            hitAxisShaft(best, ray, frame, 0, GizmoHandle::AxisX,
                         kShaftLength + kConeLength + 0.05f);
            hitAxisShaft(best, ray, frame, 1, GizmoHandle::AxisY,
                         kShaftLength + kConeLength + 0.05f);
            hitAxisShaft(best, ray, frame, 2, GizmoHandle::AxisZ,
                         kShaftLength + kConeLength + 0.05f);

            static constexpr GizmoHandle planeHandles[3] = {
                GizmoHandle::PlaneYZ, GizmoHandle::PlaneXZ, GizmoHandle::PlaneXY};
            for (int i = 0; i < 3; ++i) {
                const int a = (i + 1) % 3;
                const int b = (i + 2) % 3;
                float t = 0.0f;
                if (!rayPlane(ray, frame.origin, frame.axes[i], t)) {
                    continue;
                }
                const glm::vec3 p = ray.at(t) - frame.origin;
                const float ca = glm::dot(p, frame.axes[a]) / frame.scale;
                const float cb = glm::dot(p, frame.axes[b]) / frame.scale;
                const float lo = kPlaneOffset - 0.03f;
                const float hi = kPlaneOffset + kPlaneSize + 0.03f;
                if (ca >= lo && ca <= hi && cb >= lo && cb <= hi) {
                    consider(best, planeHandles[i], 3, t);
                }
            }
            break;
        }
        case TransformGizmo::Mode::Rotate: {
            static constexpr GizmoHandle ringHandles[3] = {
                GizmoHandle::RingX, GizmoHandle::RingY, GizmoHandle::RingZ};
            for (int i = 0; i < 3; ++i) {
                float t = 0.0f;
                if (!rayPlane(ray, frame.origin, frame.axes[i], t)) {
                    continue;
                }
                const float r = glm::length(ray.at(t) - frame.origin) / frame.scale;
                if (std::abs(r - kRingRadius) <= kRingPickBand) {
                    consider(best, ringHandles[i], 1, t);
                }
            }
            break;
        }
        case TransformGizmo::Mode::Scale: {
            hitAxisShaft(best, ray, frame, 0, GizmoHandle::ScaleX,
                         kShaftLength + 2.0f * kScaleCube + 0.05f);
            hitAxisShaft(best, ray, frame, 1, GizmoHandle::ScaleY,
                         kShaftLength + 2.0f * kScaleCube + 0.05f);
            hitAxisShaft(best, ray, frame, 2, GizmoHandle::ScaleZ,
                         kShaftLength + 2.0f * kScaleCube + 0.05f);

            // Uniform center: distance of the ray to the origin
            float tRay = glm::dot(frame.origin - ray.origin, ray.direction);
            if (tRay > 0.0f) {
                const float d = glm::length(ray.at(tRay) - frame.origin);
                if (d <= 1.8f * kUniformCube * frame.scale) {
                    consider(best, GizmoHandle::ScaleUniform, 3, tRay);
                }
            }
            break;
        }
    }
    return best.handle;
}

}  // namespace editing
