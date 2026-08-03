# src/vulkan/

`QuantiloomVulkanWindow` (a `QVulkanWindow`) owns `QuantiloomVulkanRenderer`, which is
the **only** place this repo talks to the SDK — everything goes through
`quantiloom::ExternalRenderContext`. Qt creates the Vulkan device and the SDK is handed
the existing handles rather than making its own.

## Every setter has the same shape

```cpp
void QuantiloomVulkanRenderer::setX(T x) {
    m_x = x;                       // kept so the value survives a scene reload
    if (m_renderContext) {
        m_renderContext->SetX(x);
        resetAccumulation();       // required: the accumulated image is now stale
    }
}
```

Sixteen call sites do this. Skipping `resetAccumulation()` leaves the progressive
buffer blending frames rendered under old and new settings, which reads as a slow fade
rather than as a bug — or, once the loop has stopped at its target, as a viewport that
does not react at all until the camera moves.

Display-stage setters are the deliberate exception. Sensor simulation and CLAHE change
how the accumulation is *shown*, not what it holds, so `setSensorEnabled`,
`setSensorParams` and `setDisplayEnhancement` call `requestDisplayReprocess()` instead:
it arms a one-shot flag and asks for a frame, and `startNextFrame()` routes that frame
through the SDK's `ReprocessAccumulated()` — post-processing re-run over the unchanged
accumulation, no sample added. `setGridVisible` is display-only too but compositing
rather than post-processing, so it just requests a frame.

## Camera angles are radians

`m_orbitYaw` and `m_orbitPitch` are radians throughout — clamped against
`glm::half_pi<float>()`, fed straight into `cos`/`sin`. Their declarations are commented
only "Horizontal angle" / "Vertical angle". Writing degrees into them has already
shipped once as a bug: the camera jumped on the first drag after loading a scene.

## Commits

**No Claude Code session link in a commit message.** No `Claude-Session:` trailer,
no `https://claude.ai/code/...` URL, in the subject, the body or a trailer. Same for
PR descriptions.
