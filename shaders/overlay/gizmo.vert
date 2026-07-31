#version 450

// Transform gizmo: pre-built world-space triangles, per-vertex color.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 vColor;

layout(push_constant) uniform PC {
    mat4 viewProj;
} pc;

void main() {
    vColor = inColor;
    gl_Position = pc.viewProj * vec4(inPosition, 1.0);
}
