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

Display-stage setters are the deliberate exception. CLAHE changes how the
accumulation is *shown*, not what it holds, so `setDisplayEnhancement` calls
`requestDisplayReprocess()` instead: it arms a one-shot flag and asks for a frame, and
`startNextFrame()` routes that frame through the SDK's `ReprocessAccumulated()` —
post-processing re-run over the unchanged accumulation, no sample added. Turning the
camera *off* is display-stage the same way. The physical camera has three tiers:
`reprocessCameraDisplay()` (`UpdateCameraDisplayConfig`) re-runs the display half of
the ISP over the last completed acquisition, with no ray retraced and the acquisition
state never advanced; a `setCameraConfig()` the core answers from the standing
measured rate (readout, noise, ADC) re-runs detector-to-display and ends in
`requestDisplayReprocess()`; and a `setCameraConfig()` that rebuilds the measurement —
a preset, the optics, the exposure — or `setSensorEnabled(true)` starts the
acquisition over with no paths in it, so it goes through `resetAccumulation()`.
Re-presenting there used to show a loop stopped at its target an acquisition that no
longer existed: a black frame, and the new one never traced. `setGridVisible` is
display-only too but compositing rather than post-processing, so it just requests a
frame.

## Camera angles are radians

`m_orbitYaw` and `m_orbitPitch` are radians throughout — clamped against
`glm::half_pi<float>()`, fed straight into `cos`/`sin`. Their declarations are commented
only "Horizontal angle" / "Vertical angle". Writing degrees into them has already
shipped once as a bug: the camera jumped on the first drag after loading a scene.

## Straight down still has a heading

`m_cameraUp` is the up the document authored: it is what a save writes and what
the SDK's motion poses are built from, so it goes to the SDK as the reference and
never as anything view-dependent. Along it, though, it says nothing about which
way the picture faces, and the SDK then falls back to one fixed axis — every
straight-down view faced the same way, so an orbit reaching the pole snapped to
that orientation and could not turn about the vertical. The orbit's own up,
`orbitViewUp()`, therefore rides along as the view-up hint of the four-argument
`SetCameraLookAt`, which the SDK reads only in that case. Send poses through
`pushCameraLookAt()`; only `setCamera()` uses the three-argument call, because a
document's camera must render as the CLI renders it. And recover angles with
`setOrbitAnglesFromView()`, not `atan2(dir.x, dir.z)`: on the pole that is
rounding noise, and the yaw it produced was what the next drag snapped to.

## Commits

**No Codex session link in a commit message.** No `Codex-Session:` trailer,
no `https://Codex.ai/code/...` URL, in the subject, the body or a trailer. Same for
PR descriptions.
