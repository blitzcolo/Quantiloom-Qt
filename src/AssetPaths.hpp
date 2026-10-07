/**
 * @file AssetPaths.hpp
 * @brief Where Studio finds the files it ships or the SDK ships beside it
 *
 * Four call sites used to carry their own copy of the same candidate list,
 * and two of them disagreed with the comments describing the order. One copy
 * lives here now, and its order is the contract the comments describe.
 */

#pragma once

#include <QString>

namespace assetpaths {

/// A file or directory Studio ships, by its path relative to the install root
/// ("assets/luts/astmg173.csv"). Beside the executable first -- the
/// QuantiloomQtAssets target deploys assets/spectral and assets/luts there, so
/// a build run from anywhere finds the copy it was built with -- then the
/// working directory, for a run from the repository root. Empty if neither exists.
[[nodiscard]] QString bundled(const QString& relativePath);

/// The NN atmosphere model pack: $QUANTILOOM_ATMOS_MODELS, then
/// <cwd>/assets/atmos_models, then <exe dir>/assets/atmos_models -- the core
/// CLI's order, which is why it is not bundled()'s. Empty if none exists.
[[nodiscard]] QString atmosphereModelPackDir();

} // namespace assetpaths
