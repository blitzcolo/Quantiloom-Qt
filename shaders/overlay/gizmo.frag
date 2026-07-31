#version 450

// Flat per-vertex color, linear (the sRGB attachment encodes on write).
// Depth-tested against the overlay's own cleared depth: the gizmo always
// covers the scene and the grid, but occludes itself correctly.

layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vColor;
}
