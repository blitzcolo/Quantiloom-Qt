/**
 * @file SdkGuard.hpp
 * @brief Startup check that this binary and the Quantiloom library it loads
 *        came from the same SDK install.
 *
 * This repository links a prebuilt SDK produced by Quantiloom-dev. The SDK's
 * public types are source-compatible across updates but not binary-compatible:
 * `SensorParams` gained `noiseSeed` and `GenericSensor` gained two members, so
 * both types changed size. Running an executable built against the old headers
 * on top of the new `Quantiloom.dll` is undefined behaviour, and nothing in the
 * build or at runtime used to report it -- the program simply misbehaves.
 *
 * CMake records the SDK library's SHA-256 into SdkStamp.hpp at configure time.
 * checkSdkBinaries() compares that against what is actually on disk.
 */

#pragma once

#include <QString>

/// Outcome of the SDK stamp comparison.
struct SdkCheckResult {
    /// True when the executable would load a library it was not built against.
    /// The caller is expected to refuse to continue.
    bool mismatch = false;

    /// True when the build is merely out of date: the loaded library still
    /// matches this binary, but the SDK has been reinstalled since. Advisory.
    bool stale = false;

    /// Human-readable explanation, empty when nothing is wrong.
    QString message;
};

/**
 * @brief Compare the loaded Quantiloom library against the one this binary was
 *        built against.
 *
 * Both checks are skipped silently when the information needed is unavailable
 * (no stamp recorded, or the library is not co-located with the executable), so
 * this is safe to call unconditionally.
 *
 * @return What, if anything, is inconsistent.
 */
SdkCheckResult checkSdkBinaries();
