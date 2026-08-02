/**
 * @file GizmoModel.hpp
 * @brief Geometry and hit-testing for the drawn transform gizmo
 *
 * Pure math: no Qt widgets, no Vulkan. The overlay pass renders the triangle
 * list this produces; the viewport window ray-tests the same shapes for hover
 * and grab. Both sides use one GizmoFrame so what you see is what you hit.
 *
 * Everything is expressed in "gizmo units" scaled by GizmoFrame::scale, which
 * is derived from camera distance so the gizmo is the same size on screen at
 * any distance (Blender behavior). Hit thresholds use the same unit, so the
 * pick radius is screen-constant too.
 *
 * Handle colors are world-axis semantics shared with the grid overlay
 * (X red, Y green, Z blue), i.e. viewport content -- deliberately not driven
 * by the UI theme.
 *
 * @author blitzcolo
 */

#pragma once

#include "TransformGizmo.hpp"
#include "../vulkan/CameraMatrices.hpp"

#include <glm/glm.hpp>

#include <vector>

namespace editing {

enum class GizmoHandle {
    None,
    AxisX, AxisY, AxisZ,          // translate arrows / scale shafts
    PlaneXY, PlaneXZ, PlaneYZ,    // translate plane quads
    RingX, RingY, RingZ,          // rotate rings
    ScaleX, ScaleY, ScaleZ,       // scale end cubes
    ScaleUniform                  // center cube
};

/// Which world/local axis a handle moves along, or the plane normal for a
/// plane handle / ring. Column index into GizmoFrame::axes.
int gizmoHandleAxis(GizmoHandle handle);

struct GizmoFrame {
    glm::vec3 origin{0.0f};   // selection center
    glm::mat3 axes{1.0f};     // columns = handle axes (identity in world space)
    float scale = 1.0f;       // world units per gizmo unit

    /// Screen-constant sizing: at distance d the viewport spans
    /// 2 * d * tan(fovY/2) world units vertically, so this keeps the gizmo at
    /// a fixed fraction of the viewport height.
    static float screenScale(const glm::vec3& cameraPos, const glm::vec3& origin,
                             float fovScale);
};

struct GizmoVertex {
    glm::vec3 position;
    glm::vec4 color;  // linear, alpha-blended
};

/// Append a wireframe box around a world-space AABB, as thin tubes on the
/// same vertex list the gizmo uses. This is what makes a selection visible:
/// with only a gizmo at the median point, a multi-selection gave no clue
/// which objects were in it.
void buildSelectionBoxGeometry(const glm::vec3& min, const glm::vec3& max,
                               const glm::vec4& color, float edgeRadius,
                               std::vector<GizmoVertex>& out);

/// Append the triangle list for `mode`'s handle set. `hovered` draws yellow,
/// `active` white; `viewDir` (camera toward origin) dims ring back halves.
void buildGizmoGeometry(TransformGizmo::Mode mode, const GizmoFrame& frame,
                        GizmoHandle hovered, GizmoHandle active,
                        const glm::vec3& viewDir,
                        std::vector<GizmoVertex>& out);

/// Ray-test the same handle set. Returns the grabbed handle, preferring
/// planes over axes over rings, nearest hit within a class.
GizmoHandle hitTestGizmo(const vkview::CameraRay& ray, const GizmoFrame& frame,
                         TransformGizmo::Mode mode);

}  // namespace editing
