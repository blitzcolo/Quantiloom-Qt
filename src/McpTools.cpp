/**
 * @file McpTools.cpp
 * @brief What Studio offers an agent
 *
 * MainWindow::registerMcpTools() and the handlers it installs. These are
 * MainWindow's own members -- split into their own translation unit because the
 * catalogue is long, not because it is a separate thing.
 *
 * **Every mutation goes through an apply* dispatcher.** That is the whole
 * reason the dispatchers exist: a tool that called the renderer directly would
 * leave the panels showing the old value, and "the renderer knows, the panel
 * does not" is the state this shell is built to prevent. It also means a tool
 * gets the undo entry, the modified marker and the status message for free.
 *
 * **Tool names and descriptions are not translated.** They are protocol
 * identifiers and API documentation for a model, not labels for a person; the
 * agent reads the same English whatever language the window is in.
 *
 * Handlers run on the GUI thread, from MainWindow::pumpMcp(). They may touch
 * widgets and the render context freely; what they may not do is assume either
 * exists, since minimising the window destroys the render context.
 *
 * @author blitzcolo
 */

#include "MainWindow.hpp"

#include "config/ConfigManager.hpp"
#include "editing/SelectionManager.hpp"
#include "editing/UndoStack.hpp"
#include "panels/AtmosphericPanel.hpp"
#include "panels/DebugVisualizationPanel.hpp"
#include "panels/MaterialEditorPanel.hpp"
#include "panels/SceneTreePanel.hpp"
#include "panels/SensorPanel.hpp"
#include "panels/SpectralConfigPanel.hpp"
#include "ui/ModeCatalog.hpp"
#include "vulkan/QuantiloomVulkanWindow.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <core/Log.hpp>
#include <io/ImageIO.hpp>
#include <scene/Scene.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace mcp = quantiloom::mcp;

namespace {

mcp::ToolResult Json(const QJsonObject& document) {
    return mcp::ToolResult::Text(
        QJsonDocument(document).toJson(QJsonDocument::Indented).toStdString());
}

mcp::ToolResult Json(const QJsonArray& document) {
    return mcp::ToolResult::Text(
        QJsonDocument(document).toJson(QJsonDocument::Indented).toStdString());
}

quantiloom::String Dump(const QJsonObject& document) {
    return QJsonDocument(document).toJson(QJsonDocument::Indented).toStdString();
}

/// Parses the arguments object, or explains why it could not.
bool ParseArgs(const quantiloom::String& argumentsJson, QJsonObject& out, mcp::ToolResult& error) {
    QJsonParseError parseError{};
    const QJsonDocument document =
        QJsonDocument::fromJson(QByteArray::fromStdString(argumentsJson), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = mcp::ToolResult::Error("arguments are not a JSON object: " +
                                       parseError.errorString().toStdString());
        return false;
    }
    out = document.object();
    return true;
}

glm::vec3 ReadVec3(const QJsonValue& value, const glm::vec3& fallback) {
    if (!value.isArray()) {
        return fallback;
    }
    const QJsonArray array = value.toArray();
    if (array.size() != 3) {
        return fallback;
    }
    return glm::vec3(static_cast<float>(array[0].toDouble()),
                     static_cast<float>(array[1].toDouble()),
                     static_cast<float>(array[2].toDouble()));
}

QJsonArray WriteVec3(const glm::vec3& v) {
    return QJsonArray{v.x, v.y, v.z};
}

/// The one sentence every tool that needs a live renderer says when there is
/// not one. Minimising Studio releases the Vulkan resources; restoring it
/// rebuilds them and replays the document.
mcp::ToolResult NoContext() {
    return mcp::ToolResult::Error(
        "Studio has no render context right now, which happens while the window is minimised. "
        "Restore the window and retry.");
}

/// The scalar view of a material's infrared curves, as the material editor
/// shows it: the first point of each curve.
float IrScalar(const quantiloom::Vector<std::pair<float, float>>& curve) {
    return curve.empty() ? 0.0f : curve[0].second;
}

}  // namespace

void MainWindow::registerMcpTools() {
    if (!m_mcpServer) {
        return;
    }

    int registered = 0;
    const auto add = [this, &registered](const mcp::ToolDef& tool) {
        if (auto added = m_mcpServer->RegisterTool(tool); added.has_value()) {
            ++registered;
        } else {
            QL_LOG_ERROR("MCP: could not register '{}': {}", tool.name, added.error());
        }
    };

    // ========================================================================
    // Reading
    // ========================================================================

    {
        mcp::ToolDef tool;
        tool.name = "ql_get_status";
        tool.description =
            "What Studio is rendering right now: accumulated and target samples, spectral mode, "
            "wavelength, debug visualisation mode, whether a scene is loaded and whether the "
            "document has unsaved changes.\n"
            "\n"
            "Cheap. Prefer it to ql_capture_viewport when a number would answer the question -- "
            "poll it to watch accumulation converge, since there is no convergence metric to wait "
            "on and 'converged' here means the sample count reached its target.";
        tool.inputSchemaJson = R"({"type":"object","properties":{}})";
        tool.readOnly = true;
        tool.handler = [this](const quantiloom::String&) {
            QJsonObject out;
            out["has_scene"] = m_vulkanWindow->getScene() != nullptr;
            out["scene_file"] = m_currentSceneFile;
            out["config_file"] = m_currentConfigFile;
            out["unsaved_changes"] = m_sceneModified;
            out["accumulated_samples"] =
                static_cast<qint64>(m_vulkanWindow->currentSampleCount());
            out["target_samples"] = static_cast<qint64>(m_vulkanWindow->targetSPP());
            out["render_paused"] = m_vulkanWindow->isRenderPaused();
            out["spectral_mode"] = catalog::spectralModeId(m_vulkanWindow->spectralMode());
            out["wavelength_nm"] = m_vulkanWindow->wavelength();
            out["debug_mode"] = catalog::debugModeId(m_vulkanWindow->debugMode());
            out["display_enhancement"] = m_displayEnhancementEnabled;
            return Json(out);
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_get_scene";
        tool.description =
            "The loaded scene's contents: nodes with their indices, materials with their indices, "
            "and triangle and vertex totals.\n"
            "\n"
            "The indices are what ql_set_node_transform and ql_set_material take. "
            "response_format 'concise' (the default) gives names and indices alone; 'detailed' "
            "adds each node's transform and each material's parameters, which is a lot of text "
            "for a large scene.";
        tool.inputSchemaJson = R"({
  "type": "object",
  "properties": {
    "response_format": {
      "type": "string",
      "enum": ["concise", "detailed"],
      "description": "How much to return per node and material. Default concise."
    }
  }
})";
        tool.readOnly = true;
        tool.handler = [this](const quantiloom::String& argumentsJson) {
            QJsonObject args;
            mcp::ToolResult error;
            if (!ParseArgs(argumentsJson, args, error)) {
                return error;
            }
            const bool detailed =
                args.value(QStringLiteral("response_format")).toString(QStringLiteral("concise")) ==
                QLatin1String("detailed");

            const quantiloom::Scene* scene = m_vulkanWindow->getScene();
            if (!scene) {
                return mcp::ToolResult::Error(
                    "No scene is loaded. Open one with ql_apply_config first.");
            }

            QJsonObject out;
            out["name"] = QString::fromStdString(scene->name);
            out["triangles"] = static_cast<qint64>(scene->GetTotalTriangleCount());
            out["vertices"] = static_cast<qint64>(scene->GetTotalVertexCount());
            out["mesh_count"] = static_cast<qint64>(scene->meshes.size());

            QJsonArray nodes;
            for (size_t i = 0; i < scene->nodes.size(); ++i) {
                const auto& node = scene->nodes[i];
                QJsonObject entry;
                entry["index"] = static_cast<qint64>(i);
                entry["name"] = QString::fromStdString(node.name);
                if (detailed) {
                    // Column-major, as glm stores it and as ql_set_node_transform
                    // expects it back.
                    QJsonArray matrix;
                    for (int c = 0; c < 4; ++c) {
                        for (int r = 0; r < 4; ++r) {
                            matrix.append(node.transform[c][r]);
                        }
                    }
                    entry["transform"] = matrix;
                }
                nodes.append(entry);
            }
            out["nodes"] = nodes;

            QJsonArray materials;
            for (size_t i = 0; i < scene->materials.size(); ++i) {
                const auto& material = scene->materials[i];
                QJsonObject entry;
                entry["index"] = static_cast<qint64>(i);
                entry["name"] = QString::fromStdString(material.name);
                if (detailed) {
                    entry["base_color"] = WriteVec3(glm::vec3(material.baseColorFactor));
                    entry["metallic"] = material.metallicFactor;
                    entry["roughness"] = material.roughnessFactor;
                    entry["emissive"] = WriteVec3(material.emissiveFactor);
                    entry["ir_emissivity"] = IrScalar(material.irEmissivityCurve);
                    entry["ir_transmittance"] = IrScalar(material.irTransmittanceCurve);
                    entry["ir_temperature_k"] = material.irTemperature_K;
                }
                materials.append(entry);
            }
            out["materials"] = materials;

            return Json(out);
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_get_config";
        tool.description =
            "The open document as TOML -- the same bytes File > Save would write, produced by the "
            "same writer.\n"
            "\n"
            "Use it to see what an edit changed, or to hand the document to the command-line "
            "renderer, which reads it the same way. Everything you can change through these "
            "tools is in it, including node transforms and material edits, except the two "
            "things that are ways of looking at a scene rather than part of it -- the debug "
            "visualisation mode and display enhancement. The reply lists those in "
            "session_only.";
        tool.inputSchemaJson = R"({"type":"object","properties":{}})";
        tool.readOnly = true;
        tool.handler = [this](const quantiloom::String&) {
            SceneConfig config;
            collectCurrentConfig(config);

            QJsonObject out;
            out["toml"] = m_configManager->exportConfigToString(config);
            out["config_file"] = m_currentConfigFile;
            out["unsaved_changes"] = m_sceneModified;
            out["session_only"] = QJsonArray{
                QStringLiteral("debug visualisation mode (ql_set_debug_mode)"),
                QStringLiteral("display enhancement (ql_set_display_enhancement)"),
            };
            return Json(out);
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_capture_viewport";
        tool.description =
            "A picture of what the viewport is showing.\n"
            "\n"
            "view 'display' (the default) is what is on screen, display enhancement included -- "
            "what a person would see. view 'raw' is the accumulated linear radiance before that, "
            "which is what the physical values look like and will often be very dark or clipped.\n"
            "\n"
            "Costs context roughly in proportion to area, so leave max_dimension at its default "
            "unless detail is the point. Reach for ql_get_status or ql_read_pixel first when a "
            "number would answer the question; use this when the question is about how it looks.\n"
            "\n"
            "Give save_path to also write the full-resolution capture to disk, as EXR or PNG "
            "chosen by extension.";
        tool.inputSchemaJson = R"({
  "type": "object",
  "properties": {
    "view": {
      "type": "string",
      "enum": ["display", "raw"],
      "description": "'display' includes display enhancement; 'raw' is the linear radiance. Default display."
    },
    "max_dimension": {
      "type": "integer",
      "minimum": 64,
      "maximum": 2048,
      "description": "Long edge of the returned image in pixels. Default 768."
    },
    "save_path": {
      "type": "string",
      "description": "Optional. Also write the full-resolution capture here (.exr or .png)."
    }
  }
})";
        tool.readOnly = true;
        tool.timeoutMs = 60000;  // the readback stalls on the GPU
        tool.handler = [this](const quantiloom::String& argumentsJson) {
            QJsonObject args;
            mcp::ToolResult error;
            if (!ParseArgs(argumentsJson, args, error)) {
                return error;
            }
            if (!m_vulkanWindow->getScene()) {
                return mcp::ToolResult::Error("No scene is loaded, so there is nothing to capture.");
            }

            const bool raw =
                args.value(QStringLiteral("view")).toString(QStringLiteral("display")) ==
                QLatin1String("raw");
            std::unique_ptr<quantiloom::Image> image =
                raw ? m_vulkanWindow->captureScreenshot() : m_vulkanWindow->captureDisplayImage();
            if (!image) {
                return NoContext();
            }

            const int maxDim = args.value(QStringLiteral("max_dimension")).toInt(768);

            QJsonObject out;
            out["width"] = static_cast<qint64>(image->width);
            out["height"] = static_cast<qint64>(image->height);
            out["view"] = raw ? QStringLiteral("raw") : QStringLiteral("display");
            out["accumulated_samples"] =
                static_cast<qint64>(m_vulkanWindow->currentSampleCount());

            const QString savePath = args.value(QStringLiteral("save_path")).toString();
            if (!savePath.isEmpty()) {
                const bool png = QFileInfo(savePath).suffix().compare(
                                     QStringLiteral("png"), Qt::CaseInsensitive) == 0;
                const bool wrote = png
                    ? quantiloom::ImageIO::WritePNG(savePath.toStdString(), *image)
                    : quantiloom::ImageIO::WriteEXR(savePath.toStdString(), *image);
                out["saved"] = wrote;
                out["save_path"] = savePath;
            }

            mcp::ToolResult result;
            result.text = Dump(out);
            result.imageBase64 =
                mcp::EncodeImageContent(*image, static_cast<quantiloom::u32>(maxDim));
            result.imageMimeType = "image/png";
            return result;
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_read_pixel";
        tool.description =
            "The value at one pixel of the render output, with the reading the current debug "
            "visualisation mode gives it.\n"
            "\n"
            "This is the cheap way to ask a quantitative question about the image -- is that "
            "surface actually black, what temperature is the shader seeing there -- without "
            "spending context on a picture. Coordinates are in render pixels with (0,0) at the "
            "top left.";
        tool.inputSchemaJson = R"({
  "type": "object",
  "properties": {
    "x": {"type": "integer", "minimum": 0},
    "y": {"type": "integer", "minimum": 0}
  },
  "required": ["x", "y"]
})";
        tool.readOnly = true;
        tool.handler = [this](const quantiloom::String& argumentsJson) {
            QJsonObject args;
            mcp::ToolResult error;
            if (!ParseArgs(argumentsJson, args, error)) {
                return error;
            }
            if (!args.contains(QStringLiteral("x")) || !args.contains(QStringLiteral("y"))) {
                return mcp::ToolResult::Error("x and y are both required.");
            }

            const int x = args.value(QStringLiteral("x")).toInt();
            const int y = args.value(QStringLiteral("y")).toInt();
            glm::vec4 value(0.0f);
            if (!m_vulkanWindow->readDebugPixel(x, y, value)) {
                return mcp::ToolResult::Error(
                    "Could not read that pixel. It may be outside the viewport, or no scene is "
                    "loaded.");
            }

            QJsonObject out;
            out["x"] = x;
            out["y"] = y;
            out["raw"] = QJsonArray{value.r, value.g, value.b, value.a};
            out["reading"] = m_vulkanWindow->formatDebugValue(value);
            out["debug_mode"] = catalog::debugModeId(m_vulkanWindow->debugMode());
            return Json(out);
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_list_debug_modes";
        tool.description =
            "Every debug visualisation this renderer has, grouped by category, with the identifier "
            "ql_set_debug_mode takes and a sentence on what each one shows.";
        tool.inputSchemaJson = R"({"type":"object","properties":{}})";
        tool.readOnly = true;
        tool.handler = [](const quantiloom::String&) {
            QJsonArray out;
            for (const auto category : catalog::debugCategories()) {
                QJsonObject entry;
                entry["category"] = catalog::debugCategoryName(category);
                QJsonArray modes;
                for (const auto mode : catalog::debugModesIn(category)) {
                    QJsonObject item;
                    item["id"] = catalog::debugModeId(mode);
                    item["shows"] = catalog::debugModeDescription(mode);
                    modes.append(item);
                }
                entry["modes"] = modes;
                out.append(entry);
            }
            return Json(out);
        };
        add(tool);
    }

    // ========================================================================
    // The document
    // ========================================================================

    {
        mcp::ToolDef tool;
        tool.name = "ql_apply_config";
        tool.description =
            "Open a scene configuration, replacing whatever is loaded.\n"
            "\n"
            "This is the whole document: geometry, illuminant, materials, atmosphere, camera, "
            "sensor. It is read by the same code the command-line renderer uses, so a "
            "configuration behaves identically in both.\n"
            "\n"
            "Slow the first time -- shaders compile, which is minutes on a cold cache. Discards "
            "unsaved changes and clears the undo history, so ask before calling it on a modified "
            "document; ql_get_status reports whether there are any.";
        tool.inputSchemaJson = R"({
  "type": "object",
  "properties": {
    "config_path": {
      "type": "string",
      "description": "Path to a .toml scene configuration, or to a model file (.gltf, .glb, .usd, .usdc, .usdz) to open on its own."
    }
  },
  "required": ["config_path"]
})";
        tool.destructive = true;
        // Runs from the event loop rather than from the pump: opening a scene
        // runs an event loop of its own while shaders compile, and nesting one
        // inside the other is a re-entrancy this shell has already paid for.
        tool.venue = mcp::ToolVenue::HostDispatch;
        tool.timeoutMs = 900000;
        tool.handler = [this](const quantiloom::String& argumentsJson) {
            QJsonObject args;
            mcp::ToolResult error;
            if (!ParseArgs(argumentsJson, args, error)) {
                return error;
            }
            const QString path = args.value(QStringLiteral("config_path")).toString();
            if (path.isEmpty()) {
                return mcp::ToolResult::Error("config_path is required.");
            }
            if (!QFileInfo::exists(path)) {
                return mcp::ToolResult::Error("No such file: " + path.toStdString());
            }

            // The same route File > Open takes, so the recent-files list, the
            // window title and the panels all end up correct without this
            // having its own opinion about any of them.
            openPath(path);

            QJsonObject out;
            out["opened"] = path;
            out["has_scene"] = m_vulkanWindow->getScene() != nullptr;
            out["accumulated_samples"] =
                static_cast<qint64>(m_vulkanWindow->currentSampleCount());

            mcp::ToolResult result;
            result.text = Dump(out);
            result.isError = m_vulkanWindow->getScene() == nullptr;
            return result;
        };
        add(tool);
    }

    // ========================================================================
    // Changing the render
    // ========================================================================

    {
        mcp::ToolDef tool;
        tool.name = "ql_set_camera";
        tool.description =
            "Move the camera. position and target are world-space points; fov_y is the vertical "
            "field of view in DEGREES. Give any subset -- what is omitted is left alone.\n"
            "\n"
            "Resets accumulation, so the sample count starts again from zero.";
        tool.inputSchemaJson = R"({
  "type": "object",
  "properties": {
    "position": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
    "target": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
    "fov_y": {"type": "number", "minimum": 1, "maximum": 179, "description": "Vertical field of view in DEGREES."}
  }
})";
        tool.handler = [this](const quantiloom::String& argumentsJson) {
            QJsonObject args;
            mcp::ToolResult error;
            if (!ParseArgs(argumentsJson, args, error)) {
                return error;
            }

            glm::vec3 position;
            glm::vec3 target;
            glm::vec3 up;
            float fovY = 45.0f;
            m_vulkanWindow->getCameraState(position, target, up, fovY);

            if (args.contains(QStringLiteral("position")) ||
                args.contains(QStringLiteral("target"))) {
                applyCameraPose(ReadVec3(args.value(QStringLiteral("position")), position),
                                ReadVec3(args.value(QStringLiteral("target")), target));
            }
            if (args.contains(QStringLiteral("fov_y"))) {
                applyCameraFov(static_cast<float>(args.value(QStringLiteral("fov_y")).toDouble()));
            }

            m_vulkanWindow->getCameraState(position, target, up, fovY);
            QJsonObject out;
            out["position"] = WriteVec3(position);
            out["target"] = WriteVec3(target);
            out["fov_y"] = fovY;
            return Json(out);
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_set_lighting";
        tool.description =
            "Change the sun and sky. Give any subset.\n"
            "\n"
            "sun_direction points FROM the surface TO the sun and is normalised for you. "
            "Radiances are RGB in W/m^2/sr -- physical units, not a 0-1 brightness: a scene lit "
            "by a measured solar spectrum sits in the hundreds, and the viewport clips above 1.0 "
            "unless the configuration normalises it. Check the result with ql_capture_viewport or "
            "ql_read_pixel rather than assuming a number looks right.\n"
            "\n"
            "Resets accumulation.";
        tool.inputSchemaJson = R"({
  "type": "object",
  "properties": {
    "sun_direction": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3,
                      "description": "From surface toward the sun. Normalised for you."},
    "sun_radiance": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3,
                     "description": "RGB, W/m^2/sr."},
    "sky_radiance": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3,
                     "description": "RGB, W/m^2/sr."},
    "shadow_rays": {"type": "boolean", "description": "Trace shadow rays for the sun."},
    "environment_map_enabled": {"type": "boolean", "description": "Whether a loaded environment map contributes light."}
  }
})";
        tool.handler = [this](const quantiloom::String& argumentsJson) {
            QJsonObject args;
            mcp::ToolResult error;
            if (!ParseArgs(argumentsJson, args, error)) {
                return error;
            }

            quantiloom::LightingParams params = *m_lightingParams;
            if (args.contains(QStringLiteral("sun_direction"))) {
                const glm::vec3 dir =
                    ReadVec3(args.value(QStringLiteral("sun_direction")), params.sunDirection);
                const float length = glm::length(dir);
                if (length > 1e-6f) {
                    params.sunDirection = dir / length;
                }
            }
            if (args.contains(QStringLiteral("sun_radiance"))) {
                params.sunRadiance_rgb =
                    ReadVec3(args.value(QStringLiteral("sun_radiance")), params.sunRadiance_rgb);
            }
            if (args.contains(QStringLiteral("sky_radiance"))) {
                params.skyRadiance_rgb =
                    ReadVec3(args.value(QStringLiteral("sky_radiance")), params.skyRadiance_rgb);
            }
            if (args.contains(QStringLiteral("shadow_rays"))) {
                params.enableShadowRays =
                    args.value(QStringLiteral("shadow_rays")).toBool() ? 1 : 0;
            }
            if (args.contains(QStringLiteral("environment_map_enabled"))) {
                params.enableEnvironmentMap =
                    args.value(QStringLiteral("environment_map_enabled")).toBool() ? 1 : 0;
            }

            applyLightingParams(params);

            const quantiloom::LightingParams& now = *m_lightingParams;
            QJsonObject out;
            out["sun_direction"] = WriteVec3(now.sunDirection);
            out["sun_radiance"] = WriteVec3(now.sunRadiance_rgb);
            out["sky_radiance"] = WriteVec3(now.skyRadiance_rgb);
            out["shadow_rays"] = now.enableShadowRays != 0;
            out["environment_map_enabled"] = now.enableEnvironmentMap != 0;
            return Json(out);
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_set_spectral";
        tool.description =
            "Change what the renderer integrates: spectral mode, single wavelength, target sample "
            "count and sampling seed. Give any subset.\n"
            "\n"
            "mode takes an identifier, not a display name: rgb, single, vis_fused, nir_fused, "
            "swir_fused, mwir_fused, lwir_fused, multispectral -- the same spellings a scene "
            "configuration uses. wavelength_nm is read only by the modes that render at one "
            "wavelength. Changing the mode or the wavelength resets accumulation; raising "
            "target_samples does not, so the render carries on from where it is.";
        tool.inputSchemaJson = R"({
  "type": "object",
  "properties": {
    "mode": {"type": "string",
             "enum": ["rgb", "single", "vis_fused", "nir_fused", "swir_fused", "mwir_fused", "lwir_fused", "multispectral"]},
    "wavelength_nm": {"type": "number", "minimum": 100, "maximum": 20000},
    "target_samples": {"type": "integer", "minimum": 1},
    "seed": {"type": "integer", "minimum": 0,
             "description": "Nonzero reproduces the same noise every time; 0 varies per frame."},
    "lambda_min_nm": {"type": "number", "description": "Hyperspectral cube range start. Document state, used when the cube is rendered."},
    "lambda_max_nm": {"type": "number"},
    "delta_lambda_nm": {"type": "number", "minimum": 0.1}
  }
})";
        tool.handler = [this](const quantiloom::String& argumentsJson) {
            QJsonObject args;
            mcp::ToolResult error;
            if (!ParseArgs(argumentsJson, args, error)) {
                return error;
            }

            if (args.contains(QStringLiteral("mode"))) {
                const quantiloom::String id =
                    args.value(QStringLiteral("mode")).toString().toStdString();
                const auto parsed = quantiloom::ParseSpectralMode(id);
                if (!parsed.has_value()) {
                    return mcp::ToolResult::Error("Not a spectral mode: " + id);
                }
                applySpectralMode(parsed.value());
            }
            if (args.contains(QStringLiteral("wavelength_nm"))) {
                applyWavelength(
                    static_cast<float>(args.value(QStringLiteral("wavelength_nm")).toDouble()));
            }
            if (args.contains(QStringLiteral("target_samples"))) {
                const int spp = args.value(QStringLiteral("target_samples")).toInt(1);
                applyTargetSpp(static_cast<uint32_t>(std::max(1, spp)));
            }
            if (args.contains(QStringLiteral("seed"))) {
                m_vulkanWindow->setSamplingSeed(
                    static_cast<uint32_t>(args.value(QStringLiteral("seed")).toInt()));
                setSceneModified(true);
            }
            if (args.contains(QStringLiteral("lambda_min_nm")) ||
                args.contains(QStringLiteral("lambda_max_nm")) ||
                args.contains(QStringLiteral("delta_lambda_nm"))) {
                // Document state the panel owns: nothing in the interactive
                // renderer reads it, the hyperspectral cube does at render
                // time, and the export reads it back from this panel.
                const float lo = static_cast<float>(
                    args.value(QStringLiteral("lambda_min_nm"))
                        .toDouble(m_spectralConfigPanel->lambdaMin()));
                const float hi = static_cast<float>(
                    args.value(QStringLiteral("lambda_max_nm"))
                        .toDouble(m_spectralConfigPanel->lambdaMax()));
                const float step = static_cast<float>(
                    args.value(QStringLiteral("delta_lambda_nm"))
                        .toDouble(m_spectralConfigPanel->deltaLambda()));
                m_spectralConfigPanel->setWavelengthRange(lo, hi, step);
                setSceneModified(true);
            }

            QJsonObject out;
            out["mode"] = catalog::spectralModeId(m_vulkanWindow->spectralMode());
            out["wavelength_nm"] = m_vulkanWindow->wavelength();
            out["target_samples"] = static_cast<qint64>(m_vulkanWindow->targetSPP());
            out["seed"] = static_cast<qint64>(m_vulkanWindow->samplingSeed());
            out["lambda_min_nm"] = m_spectralConfigPanel->lambdaMin();
            out["lambda_max_nm"] = m_spectralConfigPanel->lambdaMax();
            out["delta_lambda_nm"] = m_spectralConfigPanel->deltaLambda();
            out["accumulated_samples"] =
                static_cast<qint64>(m_vulkanWindow->currentSampleCount());
            return Json(out);
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_set_debug_mode";
        tool.description =
            "Switch the viewport to a debug visualisation, or back to normal rendering with "
            "'none'.\n"
            "\n"
            "This is how you ask the renderer what it thinks rather than what it draws: normals, "
            "albedo, roughness, the BRDF terms, temperature, emissivity. Pair it with "
            "ql_read_pixel, which reports the value under the current mode's own reading. Session "
            "state -- not part of the document, and not saved.\n"
            "\n"
            "Call ql_list_debug_modes for the identifiers.";
        tool.inputSchemaJson = R"({
  "type": "object",
  "properties": {
    "mode": {"type": "string", "description": "Identifier from ql_list_debug_modes, or 'none'."}
  },
  "required": ["mode"]
})";
        tool.handler = [this](const quantiloom::String& argumentsJson) {
            QJsonObject args;
            mcp::ToolResult error;
            if (!ParseArgs(argumentsJson, args, error)) {
                return error;
            }
            const QString id = args.value(QStringLiteral("mode")).toString();
            if (id.isEmpty()) {
                return mcp::ToolResult::Error("mode is required.");
            }

            const auto parsed = catalog::debugModeFromId(id.toStdString());
            if (!parsed.has_value()) {
                return mcp::ToolResult::Error(
                    "Not a debug mode: " + id.toStdString() +
                    ". Call ql_list_debug_modes for the list.");
            }
            applyDebugMode(parsed.value());

            QJsonObject out;
            out["mode"] = catalog::debugModeId(m_vulkanWindow->debugMode());
            out["shows"] = catalog::debugModeDescription(m_vulkanWindow->debugMode());
            out["session_only"] = true;
            return Json(out);
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_set_material";
        tool.description =
            "Change one material. index comes from ql_get_scene; give any subset of the "
            "parameters.\n"
            "\n"
            "The infrared trio matters only in the thermal modes: emissivity and transmittance "
            "are 0-1 fractions, temperature is in kelvin. Undoable with ql_undo, and written to "
            "the document as a [[materials]] entry so it survives a save and renders the same "
            "way from the command line.";
        tool.inputSchemaJson = R"({
  "type": "object",
  "properties": {
    "index": {"type": "integer", "minimum": 0},
    "base_color": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
    "metallic": {"type": "number", "minimum": 0, "maximum": 1},
    "roughness": {"type": "number", "minimum": 0, "maximum": 1},
    "emissive": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
    "ir_emissivity": {"type": "number", "minimum": 0, "maximum": 1},
    "ir_transmittance": {"type": "number", "minimum": 0, "maximum": 1},
    "ir_temperature_k": {"type": "number", "minimum": 0, "description": "Kelvin."}
  },
  "required": ["index"]
})";
        tool.handler = [this](const quantiloom::String& argumentsJson) {
            QJsonObject args;
            mcp::ToolResult error;
            if (!ParseArgs(argumentsJson, args, error)) {
                return error;
            }
            const quantiloom::Scene* scene = m_vulkanWindow->getScene();
            if (!scene) {
                return mcp::ToolResult::Error("No scene is loaded.");
            }
            if (!args.contains(QStringLiteral("index"))) {
                return mcp::ToolResult::Error("index is required.");
            }
            const int index = args.value(QStringLiteral("index")).toInt(-1);
            if (index < 0 || static_cast<size_t>(index) >= scene->materials.size()) {
                return mcp::ToolResult::Error(
                    "No material with index " + std::to_string(index) + "; the scene has " +
                    std::to_string(scene->materials.size()) + ".");
            }

            quantiloom::Material material = scene->materials[static_cast<size_t>(index)];
            if (args.contains(QStringLiteral("base_color"))) {
                const glm::vec3 rgb = ReadVec3(args.value(QStringLiteral("base_color")),
                                               glm::vec3(material.baseColorFactor));
                material.baseColorFactor = glm::vec4(rgb, material.baseColorFactor.a);
            }
            if (args.contains(QStringLiteral("metallic"))) {
                material.metallicFactor =
                    static_cast<float>(args.value(QStringLiteral("metallic")).toDouble());
            }
            if (args.contains(QStringLiteral("roughness"))) {
                material.roughnessFactor =
                    static_cast<float>(args.value(QStringLiteral("roughness")).toDouble());
            }
            if (args.contains(QStringLiteral("emissive"))) {
                material.emissiveFactor =
                    ReadVec3(args.value(QStringLiteral("emissive")), material.emissiveFactor);
            }

            // Same conversion the material editor does, from the same function:
            // the infrared trio is stored as curves and the reflectance is
            // derived from the other two rather than given.
            const float emissivity = static_cast<float>(
                args.value(QStringLiteral("ir_emissivity"))
                    .toDouble(IrScalar(material.irEmissivityCurve)));
            const float transmittance = static_cast<float>(
                args.value(QStringLiteral("ir_transmittance"))
                    .toDouble(IrScalar(material.irTransmittanceCurve)));
            const float temperatureK = static_cast<float>(
                args.value(QStringLiteral("ir_temperature_k")).toDouble(material.irTemperature_K));
            MaterialEditorPanel::applyIrScalars(material, emissivity, transmittance, temperatureK);

            applyMaterial(index, material);

            QJsonObject out;
            out["index"] = index;
            out["name"] = QString::fromStdString(material.name);
            out["base_color"] = WriteVec3(glm::vec3(material.baseColorFactor));
            out["metallic"] = material.metallicFactor;
            out["roughness"] = material.roughnessFactor;
            out["ir_emissivity"] = IrScalar(material.irEmissivityCurve);
            out["ir_transmittance"] = IrScalar(material.irTransmittanceCurve);
            out["ir_temperature_k"] = material.irTemperature_K;
            return Json(out);
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_set_node_transform";
        tool.description =
            "Move, rotate or scale one object. index comes from ql_get_scene.\n"
            "\n"
            "Give translation, rotation_euler_degrees and scale to build the transform, or give "
            "matrix directly as 16 column-major numbers. The transform is absolute, not relative "
            "to where the model file put the node -- read the current one from ql_get_scene with "
            "response_format 'detailed' if you mean to compose with it.\n"
            "\n"
            "Rebuilds the acceleration structure and resets accumulation. Undoable with ql_undo, "
            "and written to the document as a [[nodes]] entry, so it survives a save and renders "
            "the same way from the command line. A node the scene file left unnamed cannot be "
            "written down.";
        tool.inputSchemaJson = R"({
  "type": "object",
  "properties": {
    "index": {"type": "integer", "minimum": 0},
    "translation": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
    "rotation_euler_degrees": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3,
                               "description": "Applied X, then Y, then Z."},
    "scale": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
    "matrix": {"type": "array", "items": {"type": "number"}, "minItems": 16, "maxItems": 16,
               "description": "Column-major 4x4. Use instead of the three above, not as well as."}
  },
  "required": ["index"]
})";
        tool.handler = [this](const quantiloom::String& argumentsJson) {
            QJsonObject args;
            mcp::ToolResult error;
            if (!ParseArgs(argumentsJson, args, error)) {
                return error;
            }
            const quantiloom::Scene* scene = m_vulkanWindow->getScene();
            if (!scene) {
                return mcp::ToolResult::Error("No scene is loaded.");
            }
            if (!args.contains(QStringLiteral("index"))) {
                return mcp::ToolResult::Error("index is required.");
            }
            const int index = args.value(QStringLiteral("index")).toInt(-1);
            if (index < 0 || static_cast<size_t>(index) >= scene->nodes.size()) {
                return mcp::ToolResult::Error(
                    "No node with index " + std::to_string(index) + "; the scene has " +
                    std::to_string(scene->nodes.size()) + ".");
            }

            glm::mat4 transform(1.0f);
            if (args.contains(QStringLiteral("matrix"))) {
                const QJsonArray m = args.value(QStringLiteral("matrix")).toArray();
                if (m.size() != 16) {
                    return mcp::ToolResult::Error("matrix needs exactly 16 numbers.");
                }
                for (int c = 0; c < 4; ++c) {
                    for (int r = 0; r < 4; ++r) {
                        transform[c][r] = static_cast<float>(m[c * 4 + r].toDouble());
                    }
                }
            } else {
                const glm::vec3 translation =
                    ReadVec3(args.value(QStringLiteral("translation")), glm::vec3(0.0f));
                const glm::vec3 euler =
                    ReadVec3(args.value(QStringLiteral("rotation_euler_degrees")), glm::vec3(0.0f));
                const glm::vec3 scale =
                    ReadVec3(args.value(QStringLiteral("scale")), glm::vec3(1.0f));

                // Degrees in, radians out. The renderer works in radians and
                // writing degrees into it has shipped as a bug here before.
                transform = glm::translate(glm::mat4(1.0f), translation);
                transform = glm::rotate(transform, glm::radians(euler.x), glm::vec3(1, 0, 0));
                transform = glm::rotate(transform, glm::radians(euler.y), glm::vec3(0, 1, 0));
                transform = glm::rotate(transform, glm::radians(euler.z), glm::vec3(0, 0, 1));
                transform = glm::scale(transform, scale);
            }

            applyNodeTransform(index, transform);

            QJsonObject out;
            out["index"] = index;
            out["name"] = QString::fromStdString(scene->nodes[static_cast<size_t>(index)].name);
            return Json(out);
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_set_display_enhancement";
        tool.description =
            "Contrast-limited adaptive histogram equalisation on the displayed image.\n"
            "\n"
            "A display transform, not a physical one: it changes what the viewport and "
            "ql_capture_viewport view 'display' show, and never the radiance underneath. Worth "
            "turning on for the infrared modes, where the physical values occupy a tiny part of "
            "the display range and the raw view looks blank. Session state -- not saved.";
        tool.inputSchemaJson = R"({
  "type": "object",
  "properties": {
    "enabled": {"type": "boolean"},
    "clip_limit": {"type": "number", "minimum": 1, "maximum": 10, "description": "Typically 2 to 4."},
    "tile_size": {"type": "integer", "enum": [4, 8, 16, 32]},
    "luminance_only": {"type": "boolean", "description": "True preserves colour; false equalises each channel."}
  }
})";
        tool.handler = [this](const quantiloom::String& argumentsJson) {
            QJsonObject args;
            mcp::ToolResult error;
            if (!ParseArgs(argumentsJson, args, error)) {
                return error;
            }
            applyClaheParams(
                args.value(QStringLiteral("enabled")).toBool(m_displayEnhancementEnabled),
                static_cast<float>(
                    args.value(QStringLiteral("clip_limit")).toDouble(m_claheClipLimit)),
                args.value(QStringLiteral("tile_size")).toInt(m_claheTileSize),
                args.value(QStringLiteral("luminance_only")).toBool(m_claheLuminanceOnly));

            QJsonObject out;
            out["enabled"] = m_displayEnhancementEnabled;
            out["clip_limit"] = m_claheClipLimit;
            out["tile_size"] = m_claheTileSize;
            out["luminance_only"] = m_claheLuminanceOnly;
            out["session_only"] = true;
            return Json(out);
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_set_atmosphere";
        tool.description =
            "Configure the neural MODTRAN-surrogate atmosphere. Give any subset.\n"
            "\n"
            "preset is the coarse control -- clear, haze, fog, rain, overcast and friends, or "
            "'disabled' -- and the named weather features override on top of whichever preset is "
            "in effect: vis_km is visibility [0.5, 50], rainrt_mm_h is rain rate [0, 50], "
            "t_ground_k is ground temperature [253, 328], rh is relative humidity [0.05, 1], "
            "p_hpa is pressure [950, 1040], h2o_scale scales water vapour [0.5, 2].\n"
            "\n"
            "Enabling it needs a model pack on disk; if the scene's configuration named one it is "
            "already set. The spectral LUT rebakes lazily before the next frame, so the first "
            "frame after a change is slower.";
        tool.inputSchemaJson = R"({
  "type": "object",
  "properties": {
    "preset": {"type": "string",
               "enum": ["disabled", "clear", "haze", "humid", "fog", "rain", "storm", "overcast", "desert"],
               "description": "Applied first; the features below override on top."},
    "enabled": {"type": "boolean"},
    "vis_km": {"type": "number", "minimum": 0.5, "maximum": 50},
    "rainrt_mm_h": {"type": "number", "minimum": 0, "maximum": 50},
    "t_ground_k": {"type": "number", "minimum": 253, "maximum": 328},
    "rh": {"type": "number", "minimum": 0.05, "maximum": 1},
    "p_hpa": {"type": "number", "minimum": 950, "maximum": 1040},
    "h2o_scale": {"type": "number", "minimum": 0.5, "maximum": 2}
  }
})";
        tool.handler = [this](const quantiloom::String& argumentsJson) {
            QJsonObject args;
            mcp::ToolResult error;
            if (!ParseArgs(argumentsJson, args, error)) {
                return error;
            }

            // Preset first, through the panel's own flow -- its combo owns the
            // preset, and its onPresetChanged re-derives the config -- and only
            // then the explicit overrides on top. The other order let the
            // preset flow re-derive the config after the overrides and quietly
            // undo them.
            if (args.contains(QStringLiteral("preset"))) {
                const QString presetName = args.value(QStringLiteral("preset")).toString();
                quantiloom::AtmosphereNNConfig probe;
                if (!probe.ApplyPreset(presetName.toStdString()) &&
                    presetName != QLatin1String("disabled")) {
                    return mcp::ToolResult::Error("Not a preset: " + presetName.toStdString());
                }
                m_atmosphericPanel->setPreset(presetName);
            }

            quantiloom::AtmosphereNNConfig config =
                m_atmosphericPanel->getAtmosphericConfig();
            if (args.contains(QStringLiteral("enabled"))) {
                config.enabled = args.value(QStringLiteral("enabled")).toBool();
            }
            if (args.contains(QStringLiteral("vis_km"))) {
                config.visKm = args.value(QStringLiteral("vis_km")).toDouble();
            }
            if (args.contains(QStringLiteral("rainrt_mm_h"))) {
                config.rainrtMmH = args.value(QStringLiteral("rainrt_mm_h")).toDouble();
            }
            if (args.contains(QStringLiteral("t_ground_k"))) {
                config.tGroundK = args.value(QStringLiteral("t_ground_k")).toDouble();
            }
            if (args.contains(QStringLiteral("rh"))) {
                config.rh = args.value(QStringLiteral("rh")).toDouble();
            }
            if (args.contains(QStringLiteral("p_hpa"))) {
                config.pHPa = args.value(QStringLiteral("p_hpa")).toDouble();
            }
            if (args.contains(QStringLiteral("h2o_scale"))) {
                config.h2oScale = args.value(QStringLiteral("h2o_scale")).toDouble();
            }

            // No model-pack guard here: the renderer resolves an empty
            // directory against QUANTILOOM_ATMOS_MODELS and the executable's
            // own assets, and disables the atmosphere with a logged warning if
            // nothing is found. Refusing earlier would refuse configurations
            // that path would have served.
            applyAtmosphere(config);

            const auto now = m_atmosphericPanel->getAtmosphericConfig();
            QJsonObject out;
            out["enabled"] = now.enabled;
            out["note"] =
                "If no atmosphere model pack is installed the renderer logs a warning and "
                "renders without one; check ql_capture_viewport if the effect matters.";
            out["preset"] = m_atmosphericPanel->preset();
            out["vis_km"] = now.visKm;
            out["rainrt_mm_h"] = now.rainrtMmH;
            out["t_ground_k"] = now.tGroundK;
            out["rh"] = now.rh;
            out["p_hpa"] = now.pHPa;
            out["h2o_scale"] = now.h2oScale;
            return Json(out);
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_set_sensor";
        tool.description =
            "Configure the GPU sensor simulation -- the imaging chain between the radiance and "
            "the displayed pixel: optics, detector, ADC and noise. Give any subset; keys match "
            "the [sensor] section of a scene configuration.\n"
            "\n"
            "This is what makes the viewport look like a camera instead of a path tracer. It "
            "applies in real time, so ql_capture_viewport view 'display' shows its effect; view "
            "'raw' never does.";
        tool.inputSchemaJson = R"({
  "type": "object",
  "properties": {
    "enabled": {"type": "boolean"},
    "focal_length_mm": {"type": "number", "minimum": 0.1},
    "f_number": {"type": "number", "minimum": 0.5},
    "pixel_pitch_um": {"type": "number", "minimum": 0.1},
    "quantum_efficiency": {"type": "number", "minimum": 0, "maximum": 1},
    "well_capacity_e": {"type": "number", "minimum": 1},
    "integration_time_s": {"type": "number", "minimum": 0},
    "bit_depth": {"type": "integer", "minimum": 1, "maximum": 32},
    "gain": {"type": "number", "minimum": 0},
    "read_noise_e_rms": {"type": "number", "minimum": 0},
    "dark_current_e_s": {"type": "number", "minimum": 0},
    "enable_poisson_noise": {"type": "boolean"},
    "enable_read_noise": {"type": "boolean"},
    "enable_dark_current": {"type": "boolean"},
    "enable_fpn": {"type": "boolean"},
    "detector_temperature_k": {"type": "number", "minimum": 0}
  }
})";
        tool.handler = [this](const quantiloom::String& argumentsJson) {
            QJsonObject args;
            mcp::ToolResult error;
            if (!ParseArgs(argumentsJson, args, error)) {
                return error;
            }

            quantiloom::SensorParams params = m_sensorPanel->getSensorParams();
            const auto setF = [&args](const char* key, float& field) {
                if (args.contains(QLatin1String(key))) {
                    field = static_cast<float>(args.value(QLatin1String(key)).toDouble());
                }
            };
            const auto setB = [&args](const char* key, bool& field) {
                if (args.contains(QLatin1String(key))) {
                    field = args.value(QLatin1String(key)).toBool();
                }
            };
            setF("focal_length_mm", params.focalLength_mm);
            setF("f_number", params.fNumber);
            setF("pixel_pitch_um", params.pixelPitch_um);
            setF("quantum_efficiency", params.quantumEfficiency);
            setF("well_capacity_e", params.wellCapacity_e);
            setF("integration_time_s", params.integrationTime_s);
            if (args.contains(QStringLiteral("bit_depth"))) {
                params.bitDepth = static_cast<quantiloom::u32>(
                    args.value(QStringLiteral("bit_depth")).toInt());
            }
            setF("gain", params.gain);
            setF("read_noise_e_rms", params.readNoise_e_rms);
            setF("dark_current_e_s", params.darkCurrent_e_s);
            setB("enable_poisson_noise", params.enablePoissonNoise);
            setB("enable_read_noise", params.enableReadNoise);
            setB("enable_dark_current", params.enableDarkCurrent);
            setB("enable_fpn", params.enableFPN);
            setF("detector_temperature_k", params.detectorTemperature_K);

            applySensorParams(params);
            if (args.contains(QStringLiteral("enabled"))) {
                applySensorEnabled(args.value(QStringLiteral("enabled")).toBool());
            }

            const auto now = m_sensorPanel->getSensorParams();
            QJsonObject out;
            out["enabled"] = m_sensorPanel->isSensorEnabled();
            out["focal_length_mm"] = now.focalLength_mm;
            out["f_number"] = now.fNumber;
            out["gain"] = now.gain;
            out["integration_time_s"] = now.integrationTime_s;
            out["bit_depth"] = static_cast<qint64>(now.bitDepth);
            out["enable_poisson_noise"] = now.enablePoissonNoise;
            out["enable_fpn"] = now.enableFPN;
            return Json(out);
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_load_environment_map";
        tool.description =
            "Load an equirectangular HDR environment map (.exr or .hdr) for image-based "
            "lighting, or turn the loaded one off with enabled=false -- off stops it "
            "contributing light but keeps it resident, so turning it back on is instant.\n"
            "\n"
            "Conversion to a prefiltered cubemap happens on load, so the call takes a moment for "
            "a large map. Resets accumulation.";
        tool.inputSchemaJson = R"({
  "type": "object",
  "properties": {
    "path": {"type": "string", "description": "Path to an .exr or .hdr equirectangular image."},
    "enabled": {"type": "boolean", "description": "Default true. False keeps the current map but stops it lighting the scene."}
  }
})";
        tool.timeoutMs = 120000;
        tool.handler = [this](const quantiloom::String& argumentsJson) {
            QJsonObject args;
            mcp::ToolResult error;
            if (!ParseArgs(argumentsJson, args, error)) {
                return error;
            }

            const QString path = args.value(QStringLiteral("path")).toString();
            const bool enabled = args.value(QStringLiteral("enabled")).toBool(true);
            if (!path.isEmpty() && !QFileInfo::exists(path)) {
                return mcp::ToolResult::Error("No such file: " + path.toStdString());
            }

            // The enable flag rides on LightingParams; set it the way the
            // lighting panel would, then load the map the way it would.
            quantiloom::LightingParams params = *m_lightingParams;
            params.enableEnvironmentMap = enabled ? 1 : 0;
            applyLightingParams(params);
            applyEnvironmentMap(path.isEmpty() && m_lastConfig ? m_lastConfig->environmentMap
                                                               : path,
                                enabled);

            QJsonObject out;
            out["environment_map"] = m_lastConfig ? m_lastConfig->environmentMap : QString();
            out["enabled"] = enabled;
            return Json(out);
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_reset_accumulation";
        tool.description =
            "Throw away the accumulated samples and start the progressive render again from zero. "
            "Every change made through the other tools does this already; call it when you want a "
            "clean count without changing anything.";
        tool.inputSchemaJson = R"({"type":"object","properties":{}})";
        tool.handler = [this](const quantiloom::String&) {
            onResetAccumulation();
            QJsonObject out;
            out["accumulated_samples"] =
                static_cast<qint64>(m_vulkanWindow->currentSampleCount());
            return Json(out);
        };
        add(tool);
    }

    // ========================================================================
    // Taking it back
    // ========================================================================

    {
        mcp::ToolDef tool;
        tool.name = "ql_undo";
        tool.description =
            "Undo the last change. Covers node transforms and material edits, whether they were "
            "made from here or by hand in the window -- one history, shared.\n"
            "\n"
            "Camera moves, lighting, spectral settings and the document itself are not in the "
            "history and cannot be undone this way; put those back by setting them again.";
        tool.inputSchemaJson = R"({"type":"object","properties":{}})";
        tool.handler = [this](const quantiloom::String&) {
            if (!m_undoStack->canUndo()) {
                return mcp::ToolResult::Error("Nothing to undo.");
            }
            const QString description = m_undoStack->undoText();
            m_undoStack->undo();
            updatePanelsFromScene();

            QJsonObject out;
            out["undone"] = description;
            out["can_undo_more"] = m_undoStack->canUndo();
            return Json(out);
        };
        add(tool);
    }

    {
        mcp::ToolDef tool;
        tool.name = "ql_redo";
        tool.description = "Redo the change ql_undo took back.";
        tool.inputSchemaJson = R"({"type":"object","properties":{}})";
        tool.handler = [this](const quantiloom::String&) {
            if (!m_undoStack->canRedo()) {
                return mcp::ToolResult::Error("Nothing to redo.");
            }
            const QString description = m_undoStack->redoText();
            m_undoStack->redo();
            updatePanelsFromScene();

            QJsonObject out;
            out["redone"] = description;
            out["can_redo_more"] = m_undoStack->canRedo();
            return Json(out);
        };
        add(tool);
    }

    QL_LOG_INFO("MCP: Studio registered {} tools", registered);
}
