#include "OverlayRenderer.hpp"

#include <rhi/qrhi.h>

#include <QFile>
#include <QImage>
#include <QtDebug>

#include <glm/gtc/type_ptr.hpp>

#include <cstddef>

namespace viewport {

namespace {

/// grid.frag's PC block, std140: five vec4.
struct GridUniforms {
    glm::vec4 camPosFov;      // xyz camera position, w tan(fovY/2)
    glm::vec4 forwardAspect;  // xyz forward, w aspect
    glm::vec4 rightAlpha;     // xyz right, w overall grid alpha
    glm::vec4 upPad;          // xyz up, w orthographic film height (0 = perspective)
    glm::vec4 ndc;            // x: +1 when clip y points up, -1 when down
};
static_assert(sizeof(GridUniforms) == 80, "GridUniforms must match grid.frag's PC block");

/// The largest handle set is well under 1000 triangles; 64 KiB has headroom.
constexpr quint32 kGizmoVertexCapacity = 64 * 1024;

QShader loadShader(const char* name) {
    QFile file(QStringLiteral(":/shaders/overlay/%1.qsb").arg(QLatin1StringView(name)));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "OverlayRenderer: missing shader" << file.fileName();
        return {};
    }
    return QShader::fromSerialized(file.readAll());
}

QRhiGraphicsPipeline::TargetBlend alphaBlend() {
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    return blend;
}

/// Null, and said so once, when the pipeline cannot be built.
std::unique_ptr<QRhiGraphicsPipeline> built(std::unique_ptr<QRhiGraphicsPipeline> pipeline,
                                            const char* what) {
    if (!pipeline->create()) {
        qWarning() << "OverlayRenderer: failed to create the" << what << "pipeline";
        return nullptr;
    }
    return pipeline;
}

}  // namespace

OverlayRenderer::OverlayRenderer() : m_readback(std::make_unique<QRhiReadbackResult>()) {}

OverlayRenderer::~OverlayRenderer() = default;

void OverlayRenderer::ensureResources(QRhi* rhi, QRhiRenderPassDescriptor* overlayPass,
                                      QRhiRenderPassDescriptor* compositePass,
                                      QRhiTexture* frameTexture, QRhiTexture* depthTexture) {
    if (!m_nearestSampler) {
        // NEAREST on purpose: depth must never be bilinearly mixed across
        // silhouettes
        m_nearestSampler.reset(rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest,
                                               QRhiSampler::None, QRhiSampler::ClampToEdge,
                                               QRhiSampler::ClampToEdge));
        m_nearestSampler->create();
        // LINEAR for the fit, as the blit it replaces used where it could
        m_linearSampler.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                              QRhiSampler::None, QRhiSampler::ClampToEdge,
                                              QRhiSampler::ClampToEdge));
        m_linearSampler->create();
        m_gridUniforms.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                            static_cast<quint32>(sizeof(GridUniforms))));
        m_gridUniforms->create();
        m_gizmoUniforms.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                             static_cast<quint32>(sizeof(glm::mat4))));
        m_gizmoUniforms->create();
        m_compositeUniforms.reset(rhi->newBuffer(QRhiBuffer::Dynamic,
                                                 QRhiBuffer::UniformBuffer,
                                                 static_cast<quint32>(sizeof(glm::vec4))));
        m_compositeUniforms->create();
        // Dynamic: QRhi keeps one copy per frame in flight, so this frame's
        // triangles never overwrite the ones the previous frame is drawing.
        m_gizmoVertices.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                             kGizmoVertexCapacity));
        m_gizmoVertices->create();
    }

    // The textures are resized in place, which QRhi follows on its own; only
    // a different texture object -- a rebuilt device -- needs new bindings.
    if (frameTexture == m_boundFrame && depthTexture == m_boundDepth) {
        return;
    }
    m_boundFrame = frameTexture;
    m_boundDepth = depthTexture;

    m_gridBindings.reset(rhi->newShaderResourceBindings());
    m_gridBindings->setBindings({
        QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage,
                                                  depthTexture, m_nearestSampler.get()),
        QRhiShaderResourceBinding::uniformBuffer(
            1, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_gridUniforms.get()),
    });
    m_gridBindings->create();

    m_gizmoBindings.reset(rhi->newShaderResourceBindings());
    m_gizmoBindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage,
                                                 m_gizmoUniforms.get()),
    });
    m_gizmoBindings->create();

    m_compositeBindings.reset(rhi->newShaderResourceBindings());
    m_compositeBindings->setBindings({
        QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage,
                                                  frameTexture, m_linearSampler.get()),
        QRhiShaderResourceBinding::uniformBuffer(
            1, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_compositeUniforms.get()),
    });
    m_compositeBindings->create();

    // Grid: a fullscreen triangle, no vertex input, alpha-blended. It ignores
    // raster depth entirely: occlusion by the scene happens in the fragment
    // shader against the depth texture, and the gizmo drawn after it always
    // covers it (the overlay depth was just cleared).
    std::unique_ptr<QRhiGraphicsPipeline> grid(rhi->newGraphicsPipeline());
    grid->setShaderStages({{QRhiShaderStage::Vertex, loadShader("grid.vert")},
                           {QRhiShaderStage::Fragment, loadShader("grid.frag")}});
    grid->setTargetBlends({alphaBlend()});
    grid->setDepthTest(false);
    grid->setDepthWrite(false);
    grid->setCullMode(QRhiGraphicsPipeline::None);
    grid->setTopology(QRhiGraphicsPipeline::Triangles);
    grid->setShaderResourceBindings(m_gridBindings.get());
    grid->setRenderPassDescriptor(overlayPass);
    m_gridPipeline = built(std::move(grid), "grid");

    // Gizmo: world-space triangles with a colour each. Depth on, against the
    // overlay's just-cleared depth buffer: the gizmo always covers scene and
    // grid, and occludes itself correctly. No culling: handles must read
    // from every side, including ring backs.
    std::unique_ptr<QRhiGraphicsPipeline> gizmo(rhi->newGraphicsPipeline());
    gizmo->setShaderStages({{QRhiShaderStage::Vertex, loadShader("gizmo.vert")},
                            {QRhiShaderStage::Fragment, loadShader("gizmo.frag")}});
    QRhiVertexInputLayout layout;
    layout.setBindings({QRhiVertexInputBinding(static_cast<quint32>(sizeof(editing::GizmoVertex)))});
    layout.setAttributes({
        QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3,
                                 static_cast<quint32>(offsetof(editing::GizmoVertex, position))),
        QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float4,
                                 static_cast<quint32>(offsetof(editing::GizmoVertex, color))),
    });
    gizmo->setVertexInputLayout(layout);
    gizmo->setTargetBlends({alphaBlend()});
    gizmo->setDepthTest(true);
    gizmo->setDepthWrite(true);
    gizmo->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
    gizmo->setCullMode(QRhiGraphicsPipeline::None);
    gizmo->setTopology(QRhiGraphicsPipeline::Triangles);
    gizmo->setShaderResourceBindings(m_gizmoBindings.get());
    gizmo->setRenderPassDescriptor(overlayPass);
    m_gizmoPipeline = built(std::move(gizmo), "gizmo");

    // Composite: the frame texture, fitted, into the swapchain image. Opaque.
    std::unique_ptr<QRhiGraphicsPipeline> composite(rhi->newGraphicsPipeline());
    composite->setShaderStages({{QRhiShaderStage::Vertex, loadShader("composite.vert")},
                                {QRhiShaderStage::Fragment, loadShader("composite.frag")}});
    composite->setCullMode(QRhiGraphicsPipeline::None);
    composite->setTopology(QRhiGraphicsPipeline::Triangles);
    composite->setShaderResourceBindings(m_compositeBindings.get());
    composite->setRenderPassDescriptor(compositePass);
    m_compositePipeline = built(std::move(composite), "composite");
}

void OverlayRenderer::prepare(QRhi* rhi, QRhiResourceUpdateBatch* updates,
                              QRhiRenderPassDescriptor* overlayPass,
                              QRhiRenderPassDescriptor* compositePass,
                              QRhiTexture* frameTexture, QRhiTexture* depthTexture,
                              const CameraMatrices& camera,
                              const std::vector<editing::GizmoVertex>& gizmoVertices,
                              bool encodeSrgb) {
    ensureResources(rhi, overlayPass, compositePass, frameTexture, depthTexture);

    const float ndcY = rhi->isYUpInNDC() ? 1.0f : -1.0f;

    GridUniforms grid{};
    grid.camPosFov = glm::vec4(camera.position(), camera.fovScale());
    grid.forwardAspect = glm::vec4(camera.forward(), camera.aspect());
    grid.rightAlpha = glm::vec4(camera.right(), 0.55f);
    // A positive height is what tells the shader to use orthographic rays;
    // perspective passes zero.
    grid.upPad = glm::vec4(camera.up(), camera.orthographic() ? camera.orthoHeight() : 0.0f);
    grid.ndc = glm::vec4(ndcY, 0.0f, 0.0f, 0.0f);
    updates->updateDynamicBuffer(m_gridUniforms.get(), 0, static_cast<quint32>(sizeof(grid)),
                                 &grid);

    const glm::mat4 viewProj = glm::make_mat4(rhi->clipSpaceCorrMatrix().constData()) *
                               camera.projGL() * camera.view();
    updates->updateDynamicBuffer(m_gizmoUniforms.get(), 0, static_cast<quint32>(sizeof(viewProj)),
                                 glm::value_ptr(viewProj));

    m_gizmoVertexCount = 0;
    if (!gizmoVertices.empty()) {
        const size_t bytes = gizmoVertices.size() * sizeof(editing::GizmoVertex);
        if (bytes <= kGizmoVertexCapacity) {
            updates->updateDynamicBuffer(m_gizmoVertices.get(), 0, static_cast<quint32>(bytes),
                                         gizmoVertices.data());
            m_gizmoVertexCount = static_cast<quint32>(gizmoVertices.size());
        } else {
            qWarning() << "OverlayRenderer: gizmo geometry exceeds buffer capacity";
        }
    }

    const glm::vec4 params(encodeSrgb ? 1.0f : 0.0f, ndcY, 0.0f, 0.0f);
    updates->updateDynamicBuffer(m_compositeUniforms.get(), 0, static_cast<quint32>(sizeof(params)),
                                 glm::value_ptr(params));
}

void OverlayRenderer::draw(QRhiCommandBuffer* cb, QSize extent) {
    const QRhiViewport whole(0.0f, 0.0f, static_cast<float>(extent.width()),
                             static_cast<float>(extent.height()));
    if (m_gridVisible && m_gridPipeline) {
        cb->setGraphicsPipeline(m_gridPipeline.get());
        cb->setViewport(whole);
        cb->setShaderResources(m_gridBindings.get());
        cb->draw(3);  // fullscreen triangle, no vertex buffer
    }
    if (m_gizmoVertexCount > 0 && m_gizmoPipeline) {
        cb->setGraphicsPipeline(m_gizmoPipeline.get());
        cb->setViewport(whole);
        cb->setShaderResources(m_gizmoBindings.get());
        const QRhiCommandBuffer::VertexInput vertices(m_gizmoVertices.get(), 0);
        cb->setVertexInput(0, 1, &vertices);
        cb->draw(m_gizmoVertexCount);
    }
}

void OverlayRenderer::composite(QRhiCommandBuffer* cb, const QRhiViewport& shown) {
    if (!m_compositePipeline) {
        return;
    }
    cb->setGraphicsPipeline(m_compositePipeline.get());
    cb->setViewport(shown);
    cb->setShaderResources(m_compositeBindings.get());
    cb->draw(3);
}

void OverlayRenderer::requestCompositedCapture(const QString& path) {
    if (!m_capturePath.isEmpty()) {
        return;  // one at a time; the pending one wins
    }
    m_capturePath = path;
    m_captureArmed = false;
}

QRhiResourceUpdateBatch* OverlayRenderer::takeCaptureReadback(QRhi* rhi,
                                                              QRhiTexture* frameTexture) {
    if (m_capturePath.isEmpty() || m_captureArmed) {
        return nullptr;
    }
    // After the overlay pass, so the capture holds the grid and the gizmo;
    // the frame texture, so it is the render extent without the bars. QRhi
    // calls completed() on this thread once the frame has finished on the GPU.
    *m_readback = QRhiReadbackResult{};
    m_readback->completed = [this] { saveCapture(); };
    QRhiResourceUpdateBatch* batch = rhi->nextResourceUpdateBatch();
    batch->readBackTexture(QRhiReadbackDescription(frameTexture), m_readback.get());
    m_captureArmed = true;
    return batch;
}

void OverlayRenderer::saveCapture() {
    const QSize size = m_readback->pixelSize;
    bool saved = false;
    if (m_readback->format == QRhiTexture::RGBA8 && !size.isEmpty() &&
        m_readback->data.size() >= qsizetype(size.width()) * size.height() * 4) {
        // RGBA8 bytes in memory order, which is QImage's RGBA8888
        const QImage image(reinterpret_cast<const uchar*>(m_readback->data.constData()),
                           size.width(), size.height(), qsizetype(size.width()) * 4,
                           QImage::Format_RGBA8888);
        saved = image.save(m_capturePath, "PNG");
    }
    if (saved) {
        qInfo() << "OverlayRenderer: composited capture saved to" << m_capturePath;
    } else {
        qWarning() << "OverlayRenderer: failed to save composited capture to" << m_capturePath;
    }
    m_readback->data.clear();
    m_capturePath.clear();
    m_captureArmed = false;
}

void OverlayRenderer::releaseResources() {
    // A readback still in flight dies with the QRhi; the request stays, and is
    // read back again on the next device.
    m_readback->completed = nullptr;
    m_captureArmed = false;

    m_compositePipeline.reset();
    m_gizmoPipeline.reset();
    m_gridPipeline.reset();
    m_compositeBindings.reset();
    m_gizmoBindings.reset();
    m_gridBindings.reset();
    m_gizmoVertices.reset();
    m_compositeUniforms.reset();
    m_gizmoUniforms.reset();
    m_gridUniforms.reset();
    m_linearSampler.reset();
    m_nearestSampler.reset();
    m_boundFrame = nullptr;
    m_boundDepth = nullptr;
    m_gizmoVertexCount = 0;
}

}  // namespace viewport
