#version 450

// The sun, drawn over the frame as a viewer aid (grid.vert's fullscreen
// triangle supplies vClip).
//
// Per fragment: rebuild the raygen shader's primary ray exactly as grid.frag
// does, and draw only where that ray is a miss in the SDK's depth AOV (-1) and
// points above the horizon -- so buildings occlude the sun and the horizon
// clips it per pixel. The angle to the sun comes from the cross product, which
// stays accurate at the disk's few-milliradian size where acos() does not.
//
// A disk of the real angular radius with linear limb darkening, a near-white
// core tinted by the beam colour, and an exponential glow that the haze
// (transmittance) dims. Output is premultiplied: the pipeline blends
// One / OneMinusSrcAlpha, so the glow adds light and the disk covers it.
// Colors are linear; the sRGB attachment encodes on write.

layout(location = 0) in vec2 vClip;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D sceneDepth;

layout(std140, binding = 1) uniform PC {
    vec4 camPosFov;             // xyz camera position, w tan(fovY/2)
    vec4 forwardAspect;         // xyz forward, w aspect
    vec4 rightPad;              // xyz right
    vec4 upOrtho;               // xyz up, w orthographic film height (0 = perspective)
    vec4 ndc;                   // x: +1 when clip y points up, -1 when down
    vec4 sunDirRadius;          // xyz toward the sun, w angular radius in radians
    vec4 sunColorTransmittance; // rgb beam colour (max component 1), w transmittance
    vec4 sunParams;             // x fraction of the disk above the horizon
} pc;

// The glow falls off by e every kGlowScale disk radii, and is dropped beyond
// kGlowReach radii, where it is below one level of an 8-bit channel.
const float kGlowScale = 6.0;
const float kGlowReach = 30.0;
const float kGlowStrength = 0.5;

void main() {
    vec3 forward = pc.forwardAspect.xyz;
    float aspect = pc.forwardAspect.w;
    float fovScale = pc.camPosFov.w;
    vec3 right = pc.rightPad.xyz;
    vec3 up = pc.upOrtho.xyz;
    float orthoHeight = pc.upOrtho.w;

    // Raygen's NDC y grows upward
    vec2 ndc = vec2(vClip.x, pc.ndc.x * vClip.y);

    vec3 dir;
    if (orthoHeight > 0.0) {
        dir = normalize(forward);
    } else {
        dir = normalize(forward +
                        ndc.x * right * fovScale * aspect +
                        ndc.y * up * fovScale);
    }

    // Below the horizon the sun is not seen, whatever the frame holds there.
    if (dir.y <= 0.0) {
        discard;
    }

    // Only a miss shows the sun; a primary hit in front of it occludes it.
    vec2 uv = vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    if (texture(sceneDepth, uv).r >= 0.0) {
        discard;
    }

    vec3 sunDir = normalize(pc.sunDirRadius.xyz);
    float cosine = dot(dir, sunDir);
    if (cosine <= 0.0) {
        discard;
    }
    float radius = pc.sunDirRadius.w;
    float r = atan(length(cross(dir, sunDir)), cosine) / radius;
    if (r > kGlowReach) {
        discard;
    }

    vec3 beam = pc.sunColorTransmittance.rgb;
    float transmittance = clamp(pc.sunColorTransmittance.w, 0.0, 1.0);
    float horizon = pc.sunParams.x;

    vec3 rgb = beam * (kGlowStrength * sqrt(transmittance) * exp(-r / kGlowScale));

    // Antialiased edge: coverage ramps over one pixel's worth of r.
    float coverage = clamp((1.0 - r) / max(fwidth(r), 1e-3) + 0.5, 0.0, 1.0);
    if (coverage > 0.0) {
        float mu = sqrt(max(1.0 - r * r, 0.0));
        float limb = 1.0 - 0.6 * (1.0 - mu);
        vec3 core = mix(beam, vec3(1.0), 0.75);
        vec3 disk = mix(beam, core, mu) * limb;
        rgb = disk * coverage + rgb * (1.0 - coverage);
    }

    outColor = vec4(rgb * horizon, coverage * horizon);
}
