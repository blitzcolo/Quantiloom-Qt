#version 450

// The composite pass: the frame texture fitted into the swapchain. A
// fullscreen triangle from gl_VertexIndex, drawn into the viewport the
// renderer computed from presentedRect(), so the bars around a held render are
// whatever the pass cleared to.
//
// uv (0,0) is the frame's top-left texel on every backend. Clip y points down
// on Vulkan and up on Metal; params.y carries which (QRhi::isYUpInNDC), the
// same convention the grid shader reads.

layout(location = 0) out vec2 vUv;

layout(std140, binding = 1) uniform Params {
    vec4 params;  // x: 1 = sRGB-encode here, y: +1 when NDC y points up, -1 when down
} u;

void main() {
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2)) * 2.0 - 1.0;
    vUv = vec2(p.x * 0.5 + 0.5, 0.5 - u.params.y * p.y * 0.5);
    gl_Position = vec4(p, 0.0, 1.0);
}
