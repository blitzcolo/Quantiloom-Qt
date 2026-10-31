#version 450

// The frame texture as it reaches the swapchain. Clamped to [0, 1] on every
// backend, which is what the SDK's blit into a UNORM target does on Vulkan, so
// a float frame texture (the Metal track's) shows the same picture.
//
// The frame texture is sRGB, so sampling it decodes to linear. An sRGB
// swapchain encodes on write; when the surface offers none, QRhi silently
// keeps a UNORM format and params.x asks this shader to encode instead --
// without it the viewport would show linear values a stop and a half dark.

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D frame;

layout(std140, binding = 1) uniform Params {
    vec4 params;  // x: 1 = sRGB-encode here, y: +1 when NDC y points up, -1 when down
} u;

// IEC 61966-2-1, the curve an sRGB attachment applies on write
vec3 encodeSrgb(vec3 c) {
    vec3 low = c * 12.92;
    vec3 high = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
    return mix(high, low, lessThanEqual(c, vec3(0.0031308)));
}

void main() {
    vec3 c = clamp(texture(frame, vUv).rgb, 0.0, 1.0);
    if (u.params.x != 0.0) {
        c = encodeSrgb(c);
    }
    fragColor = vec4(c, 1.0);
}
