/**
 * @file OverlayRenderer.hpp
 * @brief The editor overlay and the composite, drawn with QRhi
 *
 * Two passes a frame, both QRhi's. The overlay pass draws onto the frame
 * texture the SDK has just written: the viewport grid, then the transform
 * gizmo and the selection box, with LOAD/STORE so the render underneath is
 * kept. It runs on every frame with an image, even with nothing to draw,
 * because it is the use QRhi's layout tracking expects next (see
 * SdkFrameBridge.hpp). The composite pass then clears the swapchain image and
 * draws the frame texture into the rectangle it is shown in -- all of it when
 * the render extent is the viewport's, fitted with its aspect kept while a
 * resize is held, which is where the bars come from.
 *
 * Scene occlusion comes from the SDK's primary-hit depth AOV, blitted into
 * the depth texture every frame: the grid fragment shader compares its own
 * ray-plane distance against it (both are distances along the same
 * normalized primary rays). A depth render buffer, cleared by each overlay
 * pass, orders overlay geometry against itself only -- which is what keeps
 * the gizmo always on top of the scene, Blender-style.
 *
 * All colors written by the overlay shaders are linear; the sRGB frame
 * texture encodes on write and blends in linear, so no shader does gamma.
 *
 * @author blitzcolo
 */

#pragma once

#include "CameraMatrices.hpp"
#include "../editing/GizmoModel.hpp"

#include <QSize>
#include <QString>
#include <QtGlobal>

#include <memory>
#include <vector>

class QRhi;
class QRhiBuffer;
class QRhiCommandBuffer;
class QRhiGraphicsPipeline;
class QRhiRenderPassDescriptor;
class QRhiResourceUpdateBatch;
class QRhiSampler;
class QRhiShaderResourceBindings;
class QRhiTexture;
class QRhiViewport;
struct QRhiReadbackResult;

namespace viewport {

class OverlayRenderer {
public:
    OverlayRenderer();
    ~OverlayRenderer();

    OverlayRenderer(const OverlayRenderer&) = delete;
    OverlayRenderer& operator=(const OverlayRenderer&) = delete;

    /// Display-only flag: flipping it must not reset accumulation (the scene
    /// is unchanged; only what is composited over it changes).
    void setGridVisible(bool visible) { m_gridVisible = visible; }
    [[nodiscard]] bool gridVisible() const { return m_gridVisible; }

    /// Ready this frame's drawing: build what is missing, and put the grid
    /// and gizmo uniforms, this frame's gizmo triangles and the composite's
    /// parameters into @p updates. @p overlayPass is the frame render
    /// target's pass descriptor, @p compositePass the swapchain's.
    /// @p encodeSrgb is set when the swapchain is not sRGB, so the composite
    /// has to encode by hand.
    void prepare(QRhi* rhi, QRhiResourceUpdateBatch* updates,
                 QRhiRenderPassDescriptor* overlayPass,
                 QRhiRenderPassDescriptor* compositePass, QRhiTexture* frameTexture,
                 QRhiTexture* depthTexture, const CameraMatrices& camera,
                 const std::vector<editing::GizmoVertex>& gizmoVertices, bool encodeSrgb);

    /// Inside the overlay pass: the grid, then the gizmo, over @p extent.
    void draw(QRhiCommandBuffer* cb, QSize extent);

    /// Inside the composite pass: the frame texture into @p shown.
    void composite(QRhiCommandBuffer* cb, const QRhiViewport& shown);

    /// Destroy every QRhi object. Call before the QRhi goes.
    void releaseResources();

    // ========================================================================
    // Composited capture (what-you-see screenshots for agents/debugging)
    // ========================================================================

    /// Queue a one-shot PNG capture of the composited frame -- the SDK's
    /// frame WITH the grid and gizmo composited over it, which the SDK-side
    /// captures cannot show, at the render extent and without the bars.
    /// Saved a few frames later, once the readback has cleared the frames in
    /// flight; poll the file.
    void requestCompositedCapture(const QString& path);

    /// For the overlay pass's endPass(): a batch reading the frame texture
    /// back when a capture is waiting for one, else null.
    [[nodiscard]] QRhiResourceUpdateBatch* takeCaptureReadback(QRhi* rhi,
                                                               QRhiTexture* frameTexture);

    /// A capture is requested or read back but not yet written. The frame
    /// loop has to keep running while this is true: the readback completes
    /// only as later frames are begun, and the loop stops on its own at the
    /// target sample count.
    [[nodiscard]] bool capturePending() const { return !m_capturePath.isEmpty(); }

private:
    void ensureResources(QRhi* rhi, QRhiRenderPassDescriptor* overlayPass,
                         QRhiRenderPassDescriptor* compositePass, QRhiTexture* frameTexture,
                         QRhiTexture* depthTexture);
    void saveCapture();

    bool m_gridVisible = false;

    std::unique_ptr<QRhiSampler> m_nearestSampler;  // depth, never interpolated
    std::unique_ptr<QRhiSampler> m_linearSampler;   // the fitted frame
    std::unique_ptr<QRhiBuffer> m_gridUniforms;
    std::unique_ptr<QRhiBuffer> m_gizmoUniforms;
    std::unique_ptr<QRhiBuffer> m_compositeUniforms;
    std::unique_ptr<QRhiBuffer> m_gizmoVertices;
    std::unique_ptr<QRhiShaderResourceBindings> m_gridBindings;
    std::unique_ptr<QRhiShaderResourceBindings> m_gizmoBindings;
    std::unique_ptr<QRhiShaderResourceBindings> m_compositeBindings;
    std::unique_ptr<QRhiGraphicsPipeline> m_gridPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_gizmoPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_compositePipeline;
    /// What the bindings were built against. The textures are resized in
    /// place, which QRhi follows on its own; a different object means a
    /// rebuilt device.
    QRhiTexture* m_boundFrame = nullptr;
    QRhiTexture* m_boundDepth = nullptr;
    quint32 m_gizmoVertexCount = 0;

    // Composited-capture state (one shot at a time)
    QString m_capturePath;      // non-empty = capture requested
    bool m_captureArmed = false;  // the readback is on its way
    std::unique_ptr<QRhiReadbackResult> m_readback;
};

}  // namespace viewport
