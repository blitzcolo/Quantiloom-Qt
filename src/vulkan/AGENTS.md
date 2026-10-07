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

`resetAccumulation()` also does two things the SDK call alone does not. It resets the
`ViewportSampleBatchScheduler`, because GPU timings still in flight describe the
previous image and would steer the first frames of the new one, and it emits
`pixelReadbackInvalidated`, which is what keeps the asynchronous hover readback from
showing a value read from a previous image. So a new setter that changes what the
accumulation holds must go through `resetAccumulation()` rather than call the SDK's
`ResetAccumulation()` directly — that skips both.

Display-stage setters are the deliberate exception. Sensor simulation and CLAHE change
how the accumulation is *shown*, not what it holds, so `setSensorEnabled` and
`setDisplayEnhancement` call `requestDisplayReprocess()` instead: it arms a one-shot
flag and asks for a frame, and `startNextFrame()` routes that frame through the SDK's
`ReprocessAccumulated()` — post-processing re-run over the unchanged accumulation, no
sample added. The physical camera works the same way, in two tiers:
`reprocessCameraDisplay()` (`UpdateCameraDisplayConfig`) is the display-only tier —
the display half of the ISP re-run over the last completed acquisition, with no ray
retraced and the acquisition state never advanced — and `setCameraConfig()` also ends
in `requestDisplayReprocess()`, whether the core could answer the edit from the
standing measured rate or had to rebuild the measurement, because either way the
image on screen no longer reflects the settings. `setGridVisible` is display-only too
but compositing rather than post-processing, so it just requests a frame.

## Camera angles are radians

`m_orbitYaw` and `m_orbitPitch` are radians throughout — clamped against
`glm::half_pi<float>()`, fed straight into `cos`/`sin`. Their declarations are commented
only "Horizontal angle" / "Vertical angle". Writing degrees into them has already
shipped once as a bug: the camera jumped on the first drag after loading a scene.

## Commits

**No Codex session link in a commit message.** No `Codex-Session:` trailer,
no `https://Codex.ai/code/...` URL, in the subject, the body or a trailer. Same for
PR descriptions.
