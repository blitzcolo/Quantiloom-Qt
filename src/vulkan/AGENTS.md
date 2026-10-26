# src/vulkan/

`QuantiloomVulkanWindow` (a `QVulkanWindow`) owns `QuantiloomVulkanRenderer`, which is
the **only** place this repo talks to the SDK — everything goes through
`quantiloom::ExternalRenderContext`. Qt creates the Vulkan device and the SDK is handed
the existing handles rather than making its own.

## Long SDK calls run off the GUI thread

Opening a config or a model, switching the spectral mode or the debug view (each
builds a pipeline variant the first time), re-solving the thermal field and the
click-time thermal probe go through `runContextJob()`. It hands the context to a
worker thread and keeps the GUI thread's event loop turning, so a 20 s thermal
scene no longer has Windows mark the window "Not Responding"; past 300 ms
`longOperationStarted` puts up the shell's modal `BusyOverlay`.

The SDK has no locking, so nothing may reach it while a job runs, and three
things make sure of that. `m_renderContext` is *moved out* to the worker, so
every path that checks for a context finds none; the window swallows
`UpdateRequest`, so no frame is started; and the shell's timers and the MCP pump
ask `isRendererBusy()` and stand down. A new caller that pokes the SDK from a
timer must do the same. Inside a QVulkanWindow callback (`initSwapChainResources`,
`startNextFrame`) the loop must not turn, so the job runs inline there — which is
why `applyDeferredSettings()` posts its replay instead of running it: the queue
usually holds the session's first scene.

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

## The viewport's size is not the render's

`m_targetExtent` is what every frame hands the SDK as its target extent, and
it follows the swapchain only through `adoptSwapchainExtent()` — called from
`resetAccumulation()`, and from a swapchain rebuild when nothing is
accumulated. A rebuilt swapchain alone (a splitter, a workspace switch, a
window resize) leaves the accumulation alone: `startNextFrame()` routes the
frame through the overlay's held frame at `m_targetExtent` and fits it into
the swapchain, aspect kept, bars cleared. Calling `Resize()` from
`initSwapChainResources()` is the bug this replaced — every step of a splitter
drag restarted the render at zero.

Two consequences. Anything that turns a pointer into a pixel goes through the
window's `toTargetPixels()`, never `devicePixelRatio()` alone, because the
image on screen may be scaled and offset; and anything sized from the target
extent may be replaced mid-frame only because `adoptSwapchainExtent()` drains
the queue first. The window also sets `PersistentResources`: Qt's default
releases the device when the window is hidden, and a minimize hides it, which
rebuilt the context and reloaded the scene on every restore.

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
`pushCameraLookAt()`. `setCamera()` is the exception: a document's camera must
render as the CLI renders it, so its hint is the document's own `camera.view_up`
(zero when the file has none), and a save writes `viewUp()` — the up on screen —
back as that key, which is how a straight-down view reopens facing the way it
was saved. And recover angles with
`setOrbitAnglesFromView()`, not `atan2(dir.x, dir.z)`: on the pole that is
rounding noise, and the yaw it produced was what the next drag snapped to.

## Commits

**No Codex session link in a commit message.** No `Codex-Session:` trailer,
no `https://Codex.ai/code/...` URL, in the subject, the body or a trailer. Same for
PR descriptions.
