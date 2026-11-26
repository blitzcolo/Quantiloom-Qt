/**
 * @file ResolutionPolicy.hpp
 * @brief What the viewport renders at, and how the render is shown
 *
 * Plain data shared by the window, the renderer and the shell. The renderer
 * turns a policy into the extent it hands the SDK; the shell shows and edits
 * it. Nothing here touches a GPU type.
 */

#pragma once

#include <QPoint>
#include <QSize>

#include <algorithm>

namespace viewport {

/// The side of a render, in pixels, the shell and the tools accept.
inline constexpr int kMinRenderSide = 16;
inline constexpr int kMaxRenderSide = 8192;

/// How the render extent is chosen.
struct ResolutionPolicy {
    enum class Mode {
        /// @c fixed, whatever the viewport does. Resizing the view never
        /// changes it and never restarts the accumulation.
        Fixed,
        /// The swapchain's device-pixel size times @c followScale, taken up
        /// when nothing is accumulated (a restart is the only time a size
        /// change costs nothing).
        Follow,
    };

    Mode mode = Mode::Fixed;
    QSize fixed{1280, 720};
    /// 1, 0.75, 0.5 or 0.25 in Follow mode.
    float followScale = 1.0f;

    friend bool operator==(const ResolutionPolicy&, const ResolutionPolicy&) = default;
};

/// @p size with each side inside [kMinRenderSide, kMaxRenderSide].
[[nodiscard]] inline QSize clampedRenderSize(const QSize& size) {
    return {std::clamp(size.width(), kMinRenderSide, kMaxRenderSide),
            std::clamp(size.height(), kMinRenderSide, kMaxRenderSide)};
}

/// How the render sits in the viewport, in device pixels. The scrollable
/// overflow on an axis is max(0, content - view).
struct Presentation {
    QSize content;       ///< the shown image: fitted, or the render extent at 1:1
    QSize view;          ///< the swapchain
    QPoint scroll;       ///< how far the view is scrolled over the content, clamped
    double zoom = 1.0;   ///< shown size over render extent; 1.0 is one pixel per pixel
};

}  // namespace viewport
