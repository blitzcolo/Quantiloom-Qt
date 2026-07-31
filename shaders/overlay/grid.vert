#version 450

// Fullscreen triangle from gl_VertexIndex: (-1,-1), (3,-1), (-1,3).
// No vertex buffer; the fragment shader does all the work.

layout(location = 0) out vec2 vClip;

void main() {
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2)) * 2.0 - 1.0;
    vClip = p;
    gl_Position = vec4(p, 0.0, 1.0);
}
