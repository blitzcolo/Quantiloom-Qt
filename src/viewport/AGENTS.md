# src/viewport/

`ViewportWindow` (a plain `QWindow` with a Vulkan surface) owns
`ViewportRenderer`, which is the **only** place this repo talks to the SDK —
everything goes through `quantiloom::ExternalRenderContext`. Studio creates
the device itself (`GpuDevice`) and imports it into an application-owned
`QRhi`; the SDK is handed the same handles rather than making its own.

The pieces, and which one to touch:

| File | Owns |
|---|---|
| `GpuDevice` | GPU suitability and device creation. QRhi cannot do this: it has no way to enable the ray tracing feature structs |
| `ViewportRenderer` | The QRhi, the swapchain, the frame textures, the frame loop, the context jobs, the camera and every setter |
| `SdkFrameBridge` | **Every Vulkan-typed SDK call** (`Create`/`InitParams`, the three frame calls, `BlitDepthTo`) and every `VkImageLayout`. Nowhere else in src/ has either |
| `OverlayRenderer` | The grid, the gizmo and the selection box as QRhi pipelines, the composite pass, the composited-capture readback |
| `CameraMatrices` | The SDK's camera as matrices and pixel rays. The projection is OpenGL's convention; a backend's comes from `clipSpaceCorrMatrix()` in front of it |

One frame: `beginFrame`, the SDK records into the frame texture between
`beginExternal()`/`endExternal()` outside any pass, the overlay pass draws
onto that texture (always, even with nothing visible), and the composite pass
clears the swapchain and fits the frame into it. The SDK never writes a
swapchain image — QRhi swapchains have no `TRANSFER_DST`, which the blit this
replaced needed.

QRhi is a **limited-compatibility** API and CMake pins Qt to 6.10.x for that
reason. Keep every QRhi type inside this directory, and re-check a different
minor against Qt's `qrhivulkan.cpp` before widening the pin.

## Long SDK calls run off the GUI thread

Opening a config or a model, switching the spectral mode or the debug view (each
builds a pipeline variant the first time), re-solving the thermal field and the
click-time thermal probe go through `runContextJob()`. It hands the context to a
worker thread and keeps the GUI thread's event loop turning, so a 20 s thermal
scene no longer has Windows mark the window "Not Responding"; past 300 ms
`longOperationStarted` puts up the shell's modal `BusyOverlay`.

The SDK has no locking, so nothing may reach it while a job runs, and four
things make sure of that. `m_renderContext` is *moved out* to the worker, so
every path that checks for a context finds none; the window swallows
`UpdateRequest` **and `Paint`**, so no frame is started and no queue is
submitted to; resize and expose only *mark* the swapchain instead of rebuilding
it (see below); and the shell's timers and the MCP pump ask `isRendererBusy()`
and stand down. A new caller that pokes the SDK from a timer must do the same.
Inside a frame or the first-time setup the loop must not turn, so the job runs
inline there — which is why `applyDeferredSettings()` posts its replay instead
of running it: the queue usually holds the session's first scene.

A surface about to be destroyed first *joins* the worker: tearing the swapchain
down waits for the device to go idle, which would otherwise race the worker's
submissions.

## Every setter has the same shape

```cpp
void ViewportRenderer::setX(T x) {
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
`renderFrame()` routes that frame through the SDK's `ReprocessAccumulated()` —
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
it follows the swapchain only through `adoptRenderExtent()` — called from
`resetAccumulation()`, and from a swapchain rebuild when nothing is
accumulated. A rebuilt swapchain alone (a splitter, a workspace switch, a
window resize) leaves the accumulation alone: `renderFrame()` composites the
frame texture at `m_targetExtent` into the swapchain, aspect kept, bars
cleared. Calling `Resize()` from the swapchain rebuild is the bug this
replaced — every step of a splitter drag restarted the render at zero.

Two consequences. Anything that turns a pointer into a pixel goes through the
window's `toTargetPixels()`, never `devicePixelRatio()` alone, because the
image on screen may be scaled and offset — `presentedRect()` feeds both the
composite's viewport and `swapchainToTarget()`, so drawing and hit-testing
cannot drift. And `adoptRenderExtent()` drains the queue (`m_rhi->finish()`)
before the SDK's `Resize()`, because that replaces images a frame in flight
may still be reading; the frame textures themselves are resized in place and
QRhi releases the old ones once no frame uses them. Note that `finish()` inside
a frame submits and restarts the command buffer, so the native command buffer
is only ever fetched *after* the camera update that can trigger this.

Nothing is released when the window is hidden or minimized: a hidden window
just skips frames, and a lost surface costs only the swapchain. The old
`QVulkanWindow` released the device on unexpose and rebuilt the context — and
the scene, and the accumulation — on every restore.

## Two trackers, one image

QRhi generates barriers from its own record of each texture's
`{layout, access, stage}`, and `setNativeLayout()` updates **only the
layout**. Nothing public sets the other two, so after the SDK writes a texture
QRhi's next barrier on it would have a stale source scope. The rule:

> External writes end with a barrier whose destination scope is
> `ALL_COMMANDS` / `MEMORY_READ | MEMORY_WRITE`, into exactly the layout
> QRhi's next use requests. That use must come next, so QRhi's barrier is a
> same-layout one rather than a transition ordered from a scope it never saw.

For the frame texture that next use is the overlay pass, which is why the pass
runs even when there is no grid and no gizmo. For the depth texture the SDK's
own barriers already do this, so the bridge adds none. The two barriers live in
`SdkFrameBridge.cpp`, as separate functions, because the backend-neutral SDK
API takes the final layout over and deletes them.

## Swapchain rebuilds are deferred

`QRhiSwapChain::createOrResize()` waits for the device to go idle, and
`QRhi::finish()` waits for the queue. Neither may run on the GUI thread while
a context job is submitting — the result is intermittent device loss, not a
clean error. So a resize or an expose only sets `m_swapchainDirty`, and
`renderFrame()` (which the window never calls while frames are held) is the
only place that rebuilds.

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
