#version 450

// Blender-style infinite grid on the y = 0 plane.
//
// Per fragment: rebuild the raygen shader's primary ray (same basis, same
// fovScale/aspect convention, same y flip), intersect it with the ground
// plane, and draw fwidth-antialiased lines at two log10 LOD levels that
// cross-fade with distance. Occlusion is a straight distance compare against
// the SDK's primary-hit depth AOV -- both values are distances along the
// same normalized ray, so no reprojection is involved. -1 in the AOV means
// the sky, where the grid is always visible.
//
// Colors are linear; the sRGB attachment encodes on write. Grid and axis
// colors are viewport content (world-axis semantics shared with the gizmo),
// not UI chrome, so the theme system deliberately does not drive them.

layout(location = 0) in vec2 vClip;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D sceneDepth;

layout(push_constant) uniform PC {
    vec4 camPosFov;      // xyz camera position, w tan(fovY/2)
    vec4 forwardAspect;  // xyz forward, w aspect
    vec4 rightAlpha;     // xyz right, w overall grid alpha
    vec4 upPad;          // xyz up
} pc;

// LINEAR values (the sRGB attachment encodes on write): the linear form of
// the intended display colors -- mid-dark gray, X-axis red, Z-axis blue.
// Writing display values directly would come out washed out.
const vec3 kGridColor = vec3(0.064, 0.064, 0.064);
const vec3 kAxisXColor = vec3(0.349, 0.010, 0.016);  // line along X (z == 0)
const vec3 kAxisZColor = vec3(0.010, 0.030, 0.268);  // line along Z (x == 0)

// 0..1 line intensity for a unit grid over `coord`, one pixel wide
float gridLine(vec2 coord) {
    vec2 g = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
    return 1.0 - min(min(g.x, g.y), 1.0);
}

void main() {
    vec3 camPos = pc.camPosFov.xyz;
    float fovScale = pc.camPosFov.w;
    vec3 forward = pc.forwardAspect.xyz;
    float aspect = pc.forwardAspect.w;
    vec3 right = pc.rightAlpha.xyz;
    float gridAlpha = pc.rightAlpha.w;
    vec3 up = pc.upPad.xyz;

    // Vulkan clip y grows downward; raygen's NDC y grows upward
    vec2 ndc = vec2(vClip.x, -vClip.y);
    vec3 dir = normalize(forward +
                         ndc.x * right * fovScale * aspect +
                         ndc.y * up * fovScale);

    if (abs(dir.y) < 1e-7) {
        discard;
    }
    float t = -camPos.y / dir.y;
    if (t <= 0.0) {
        discard;  // plane is behind the camera for this pixel
    }

    // Scene occlusion: same metric (distance along the normalized primary
    // ray), tolerance grows with distance to avoid grazing-angle z-fighting
    vec2 uv = vClip * 0.5 + 0.5;
    float sceneT = texture(sceneDepth, uv).r;
    if (sceneT >= 0.0 && t > sceneT + max(0.001, 0.002 * t)) {
        discard;
    }

    vec3 p = camPos + t * dir;

    // Two LOD levels from log10 of view distance: the fine grid fades out as
    // the coarse one takes over, so on-screen density stays roughly constant
    float lod = log(max(t * fovScale, 1e-6)) / log(10.0);
    float lodF = fract(lod);
    float cellFine = pow(10.0, floor(lod));
    float cellCoarse = cellFine * 10.0;

    float lineFine = gridLine(p.xz / cellFine);
    float lineCoarse = gridLine(p.xz / cellCoarse);
    float intensity = max(lineCoarse, lineFine * (1.0 - lodF));

    // Axis lines override the grid gray at full strength
    vec2 axisDist = abs(p.xz) / fwidth(p.xz);
    vec3 color = kGridColor;
    float axisBoost = 0.0;
    if (axisDist.y < 1.5) {  // z ~= 0: the X axis
        color = kAxisXColor;
        axisBoost = 1.0 - min(axisDist.y / 1.5, 1.0);
    }
    if (axisDist.x < 1.5) {  // x ~= 0: the Z axis
        color = kAxisZColor;
        axisBoost = max(axisBoost, 1.0 - min(axisDist.x / 1.5, 1.0));
    }
    intensity = max(intensity, axisBoost);

    // Grazing-angle and radial fades keep the horizon from aliasing into a
    // solid band. The radius follows the camera height -- continuous in t,
    // unlike a cell-size-based radius, which pops every time the LOD
    // crosses a power of ten
    float fade = clamp(abs(dir.y) * 6.0, 0.0, 1.0);
    float fadeRadius = 500.0 * max(abs(camPos.y), 0.5);
    fade *= 1.0 - smoothstep(0.45, 1.0, t / fadeRadius);

    float alpha = intensity * fade * gridAlpha;
    if (alpha <= 0.003) {
        discard;
    }
    outColor = vec4(color, alpha);
}
