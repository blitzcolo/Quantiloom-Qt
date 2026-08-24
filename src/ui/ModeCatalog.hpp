/**
 * @file ModeCatalog.hpp
 * @brief One source of truth for the debug and spectral mode vocabularies
 *
 * Both vocabularies used to exist twice: once as combo box items inside a
 * panel and once as a bare English `switch` in MainWindow that fed the status
 * bar. The two drifted, and the status bar half was untranslatable by
 * construction. Everything that needs a mode name — the View menu, the
 * toolbar combo, the panel, the status bar, the viewport badge, the help page
 * — now asks here.
 *
 * The names are looked up on every call rather than cached, so a language
 * switch at runtime is picked up by simply re-reading them.
 */

#pragma once

#include <QString>
#include <QVector>

#include <optional>
#include <string>

#include <core/Types.hpp>

namespace catalog {

// ============================================================================
// Debug visualization
// ============================================================================

/// The seven pipeline stages the debug modes group into, plus the "off" entry
/// and the mesh-corruption diagnostics. Used to build the View menu submenus
/// and the optgroup-style separators in the toolbar combo.
enum class DebugCategory {
    Geometry,
    Material,
    Lighting,
    BRDF,
    IBL,
    Spectral,
    Infrared,
    Diagnostics
};

/// All categories, in display order.
[[nodiscard]] QVector<DebugCategory> debugCategories();

/// Translated category heading, e.g. "Geometry".
[[nodiscard]] QString debugCategoryName(DebugCategory category);

/// The modes belonging to one category, in display order. Never contains None.
[[nodiscard]] QVector<quantiloom::DebugVisualizationMode> debugModesIn(DebugCategory category);

/// Translated mode name, e.g. "Shaded Normal". Also answers for None.
[[nodiscard]] QString debugModeName(quantiloom::DebugVisualizationMode mode);

/// One or two sentences explaining what the mode shows.
[[nodiscard]] QString debugModeDescription(quantiloom::DebugVisualizationMode mode);

/// The static "how to read a debug image" reference, as rich text. Lives in
/// the Help menu; it used to occupy half of the debug panel.
[[nodiscard]] QString debugOutputInterpretation();

/// Stable identifier, e.g. "shaded_normal". Never translated and never shown:
/// this is what an API says when it means a mode, because debugModeName()
/// changes with the interface language and a protocol cannot.
[[nodiscard]] const char* debugModeId(quantiloom::DebugVisualizationMode mode);

/// The inverse. Nothing for an identifier this build does not have.
[[nodiscard]] std::optional<quantiloom::DebugVisualizationMode> debugModeFromId(
    const std::string& id);

// ============================================================================
// Spectral mode
// ============================================================================

/// All selectable spectral modes, in display order.
[[nodiscard]] QVector<quantiloom::SpectralMode> spectralModes();

/// Short name for badges and status text, e.g. "LWIR". Band acronyms are
/// deliberately left untranslated (see the glossary in src/i18n/CLAUDE.md).
[[nodiscard]] QString spectralModeName(quantiloom::SpectralMode mode);

/// Name plus the wavelength range actually integrated, for menus and combos.
[[nodiscard]] QString spectralModeLabel(quantiloom::SpectralMode mode);

/// The band token the spectral database is indexed by -- "VIS", "NIR", "SWIR",
/// "MWIR" or "LWIR" -- for the mode currently on screen.
///
/// Protocol rather than display: SpectralIO::ReconstructBasisCurve matches it
/// literally, so this is never translated and never the label a user reads.
/// The modes that integrate no band of their own (RGB, Single, Multispectral)
/// answer VIS, which is the band a base colour describes.
///
/// It is a dispatcher for the same reason the apply* functions are. The
/// mapping used to be written out where it was needed, and the copy stopped at
/// SWIR: a material assigned while the viewport rendered MWIR was reconstructed
/// over 380-780 nm and bound anyway, and since the shader derives long-wave
/// emissivity from a bound curve, the scene then rendered from a spectrum
/// measured in the wrong half of the spectrum.
[[nodiscard]] QString spectralLibraryBand(quantiloom::SpectralMode mode);

/// One or two sentences on what the mode is for.
[[nodiscard]] QString spectralModeDescription(quantiloom::SpectralMode mode);

/// True for the fused IR bands, which currently render from RGB-averaged
/// albedo and are therefore not quantitatively meaningful.
[[nodiscard]] bool spectralModeIsPreviewOnly(quantiloom::SpectralMode mode);

/// The wavelength range a fused mode integrates, formatted for display, or an
/// empty string for modes that are not fused bands.
[[nodiscard]] QString fusedRangeText(quantiloom::SpectralMode mode);

/// Stable identifier, e.g. "lwir_fused" -- the spelling a scene configuration
/// uses and quantiloom::ParseSpectralMode reads back, so an agent that saw one
/// here can write it into a TOML document unchanged.
[[nodiscard]] const char* spectralModeId(quantiloom::SpectralMode mode);

// ============================================================================
// Render quality presets
// ============================================================================

struct QualityPreset {
    const char* id;      ///< stable key, never shown
    uint32_t spp;
};

/// Preview → Production, in ascending cost order.
[[nodiscard]] QVector<QualityPreset> qualityPresets();

/// Translated preset name including its sample count, e.g. "Preview (1 spp)".
[[nodiscard]] QString qualityPresetLabel(const QualityPreset& preset);

} // namespace catalog
