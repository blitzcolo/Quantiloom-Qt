/**
 * @file LightingPanel.cpp
 * @brief Sun/sky lighting parameter editor implementation
 */

#include "LightingPanel.hpp"

#include "../ui/ModeCatalog.hpp"
#include "../ui/UiStyle.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QStandardItemModel>
#include <algorithm>
#include <cmath>

#include <renderer/LightingParams.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

/// Ceiling of the sun and sky spin boxes. Not a physical bound, only a widget
/// one: loading an absolute spectrum puts the sun in the hundreds -- the core
/// derives linear sRGB (164.8, 138.6, 112.1) from un-normalised ASTM G-173 --
/// and the old ceilings of 100 and 10 showed the clamp instead of the value.
constexpr double kMaxStrength = 100000.0;

/// Direction FROM the surface TO the sun.
/// Azimuth: 0 = North (+Z), 90 = East (+X), 180 = South (-Z), 270 = West (-X).
glm::vec3 sunDirectionFromAngles(float azimuthDeg, float elevationDeg) {
    const float azRad = azimuthDeg * static_cast<float>(M_PI) / 180.0f;
    const float elRad = elevationDeg * static_cast<float>(M_PI) / 180.0f;
    const float cosEl = std::cos(elRad);
    return glm::normalize(glm::vec3(cosEl * std::sin(azRad),
                                    std::sin(elRad),
                                    cosEl * std::cos(azRad)));
}

} // namespace

LightingPanel::LightingPanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());
    setupUi();
    retranslateUi();
}

QString LightingPanel::panelTitle() const {
    return tr("Lighting");
}

void LightingPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // Sun direction
    m_sunDirGroup = new QGroupBox(this);
    auto* sunDirLayout = new QFormLayout(m_sunDirGroup);

    auto* azimuthRow = new QHBoxLayout();
    m_azimuthSlider = new QSlider(Qt::Horizontal);
    m_azimuthSlider->setRange(0, 360);
    m_azimuthSlider->setValue(180);
    // The slider is for dragging, the spin box for typing an exact angle; a
    // config can hold a fractional one, and the slider alone would truncate it.
    m_azimuthSpin = new QDoubleSpinBox();
    m_azimuthSpin->setRange(0.0, 360.0);
    m_azimuthSpin->setDecimals(1);
    m_azimuthSpin->setSingleStep(1.0);
    m_azimuthSpin->setWrapping(true);
    m_azimuthSpin->setSuffix(QStringLiteral("°"));   // unit symbol, not translated
    m_azimuthSpin->setKeyboardTracking(false);
    m_azimuthSpin->setValue(180.0);
    connect(m_azimuthSlider, &QSlider::valueChanged, this, &LightingPanel::onSunAzimuthChanged);
    connect(m_azimuthSlider, &QSlider::sliderPressed, this, &LightingPanel::editGestureStarted);
    connect(m_azimuthSlider, &QSlider::sliderReleased, this, &LightingPanel::editGestureFinished);
    connect(m_azimuthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LightingPanel::onSunAzimuthSpinChanged);
    azimuthRow->addWidget(m_azimuthSlider, 1);
    azimuthRow->addWidget(m_azimuthSpin);
    m_azimuthCaption = new QLabel(m_sunDirGroup);
    sunDirLayout->addRow(m_azimuthCaption, azimuthRow);

    auto* elevationRow = new QHBoxLayout();
    m_elevationSlider = new QSlider(Qt::Horizontal);
    m_elevationSlider->setRange(0, 90);
    m_elevationSlider->setValue(45);
    m_elevationSpin = new QDoubleSpinBox();
    m_elevationSpin->setRange(0.0, 90.0);
    m_elevationSpin->setDecimals(1);
    m_elevationSpin->setSingleStep(1.0);
    m_elevationSpin->setSuffix(QStringLiteral("°"));
    m_elevationSpin->setKeyboardTracking(false);
    m_elevationSpin->setValue(45.0);
    connect(m_elevationSlider, &QSlider::valueChanged, this, &LightingPanel::onSunElevationChanged);
    connect(m_elevationSlider, &QSlider::sliderPressed, this, &LightingPanel::editGestureStarted);
    connect(m_elevationSlider, &QSlider::sliderReleased, this, &LightingPanel::editGestureFinished);
    connect(m_elevationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LightingPanel::onSunElevationSpinChanged);
    elevationRow->addWidget(m_elevationSlider, 1);
    elevationRow->addWidget(m_elevationSpin);
    m_elevationCaption = new QLabel(m_sunDirGroup);
    sunDirLayout->addRow(m_elevationCaption, elevationRow);

    mainLayout->addWidget(m_sunDirGroup);

    // Radiance
    m_radianceGroup = new QGroupBox(this);
    auto* radianceLayout = new QFormLayout(m_radianceGroup);

    m_sunIntensitySpin = new QDoubleSpinBox();
    m_sunIntensitySpin->setRange(0.0, kMaxStrength);
    m_sunIntensitySpin->setSingleStep(0.1);
    m_sunIntensitySpin->setValue(1.0);
    m_sunIntensitySpin->setKeyboardTracking(false);
    connect(m_sunIntensitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LightingPanel::onSunIntensityChanged);
    m_sunCaption = new QLabel(m_radianceGroup);
    radianceLayout->addRow(m_sunCaption, m_sunIntensitySpin);

    m_skyIntensitySpin = new QDoubleSpinBox();
    m_skyIntensitySpin->setRange(0.0, kMaxStrength);
    m_skyIntensitySpin->setSingleStep(0.01);
    m_skyIntensitySpin->setValue(0.1);
    m_skyIntensitySpin->setKeyboardTracking(false);
    connect(m_skyIntensitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LightingPanel::onSkyIntensityChanged);
    m_skyCaption = new QLabel(m_radianceGroup);
    radianceLayout->addRow(m_skyCaption, m_skyIntensitySpin);

    mainLayout->addWidget(m_radianceGroup);

    // Environment map (image-based lighting)
    m_envGroup = new QGroupBox(this);
    auto* envLayout = new QVBoxLayout(m_envGroup);

    m_envEnabledCheck = new QCheckBox(m_envGroup);
    m_envEnabledCheck->setChecked(true);
    connect(m_envEnabledCheck, &QCheckBox::toggled,
            this, &LightingPanel::onEnvironmentEnabledToggled);
    envLayout->addWidget(m_envEnabledCheck);

    m_envPathLabel = new QLabel(m_envGroup);
    // The path can be far wider than the dock. Elided here, whole in the
    // tooltip; without a minimum the label would instead widen the dock.
    m_envPathLabel->setMinimumWidth(1);
    m_envPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    envLayout->addWidget(m_envPathLabel);

    auto* envButtons = new QHBoxLayout();
    m_envBrowseButton = new QPushButton(m_envGroup);
    connect(m_envBrowseButton, &QPushButton::clicked,
            this, &LightingPanel::onBrowseEnvironmentMap);
    envButtons->addWidget(m_envBrowseButton);

    m_envClearButton = new QPushButton(m_envGroup);
    connect(m_envClearButton, &QPushButton::clicked,
            this, &LightingPanel::onClearEnvironmentMap);
    envButtons->addWidget(m_envClearButton);
    envButtons->addStretch();
    envLayout->addLayout(envButtons);

    // An HDRI sky carries its own sun, so lighting a scene with one *and* an
    // analytic sun counts the same illumination twice -- and since nothing
    // aligns the two directions, the usual symptom is two specular highlights
    // in different places. Same notice style as the spectral panel's
    // preview-only warning, because it says the same kind of thing: the render
    // is not quantitative as configured.
    m_doubleCountNotice = new QLabel(m_envGroup);
    m_doubleCountNotice->setWordWrap(true);
    bindStyle([this] { uistyle::applyNoticeStyle(m_doubleCountNotice); });
    m_doubleCountNotice->setVisible(false);
    envLayout->addWidget(m_doubleCountNotice);

    // Added after the illuminant group, below: the illuminant is the sun's
    // spectrum and belongs next to the sun's radiance, not after the map.

    // ------------------------------------------------------------------
    // Illuminant
    // ------------------------------------------------------------------
    // The sun's *spectrum*, as distinct from the RGB radiance above. Without
    // one, the quantitative spectral modes render black -- the core refuses to
    // substitute a standard spectrum, because a scene that silently acquired
    // one would report radiance nobody asked for. Until now the only way to
    // supply it was to hand-edit lighting.solar_lut into the TOML.
    m_illuminantGroup = new QGroupBox(this);
    auto* illuminantLayout = new QVBoxLayout(m_illuminantGroup);

    m_illuminantCombo = new QComboBox(m_illuminantGroup);
    // Data carries the kind; the display text is translated around it.
    m_illuminantCombo->addItem(QString(), QStringLiteral("none"));
    m_illuminantCombo->addItem(QString(), QStringLiteral("equal_energy"));
    m_illuminantCombo->addItem(QString(), QStringLiteral("astm"));
    m_illuminantCombo->addItem(QString(), QStringLiteral("file"));
    connect(m_illuminantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LightingPanel::onIlluminantKindChanged);
    illuminantLayout->addWidget(m_illuminantCombo);

    m_illuminantPathLabel = new QLabel(m_illuminantGroup);
    m_illuminantPathLabel->setMinimumWidth(1);
    m_illuminantPathLabel->setVisible(false);
    bindStyle([this] { uistyle::applyHintStyle(m_illuminantPathLabel); });
    illuminantLayout->addWidget(m_illuminantPathLabel);

    m_illuminantBrowseButton = new QPushButton(m_illuminantGroup);
    m_illuminantBrowseButton->setVisible(false);
    connect(m_illuminantBrowseButton, &QPushButton::clicked,
            this, &LightingPanel::onBrowseIlluminant);
    illuminantLayout->addWidget(m_illuminantBrowseButton);

    // Reference spectra are published relative -- D65 is normalised to 100 at
    // 560 nm -- so their absolute level is arbitrary and this is usually what
    // is wanted. Both curves scale by the sun's luminance, keeping the ratio.
    m_normaliseCheck = new QCheckBox(m_illuminantGroup);
    m_normaliseCheck->setChecked(true);
    connect(m_normaliseCheck, &QCheckBox::toggled,
            this, &LightingPanel::onNormaliseToggled);
    illuminantLayout->addWidget(m_normaliseCheck);

    m_illuminantNotice = new QLabel(m_illuminantGroup);
    m_illuminantNotice->setWordWrap(true);
    m_illuminantNotice->setVisible(false);
    bindStyle([this] { uistyle::applyNoticeStyle(m_illuminantNotice); });
    illuminantLayout->addWidget(m_illuminantNotice);

    mainLayout->addWidget(m_illuminantGroup);
    mainLayout->addWidget(m_envGroup);
    mainLayout->addStretch();
}

namespace {

/// The span of the bundled ASTM G-173 table, assets/luts/astmg173.csv. Written
/// down because the panel does not read spectrum files -- and because it is the
/// whole point of the coverage notice that this stops well below the thermal
/// bands.
constexpr float kAstmMinNm = 280.0f;
constexpr float kAstmMaxNm = 4000.0f;

/// The direct beam's CIE luminance: what "normalise to unit luminance" divides
/// the ASTM curves by. Quoted in the notice so the cost is a number rather than
/// an adjective. Integrating the bundled table against the renderer's own CIE
/// table gives linear sRGB (164.8, 138.6, 112.1), Y = 142.2, at chromaticity
/// x = 0.3391 y = 0.3501 -- which is the figure SpectralData.hpp quotes for the
/// same curve, so the two agree.
constexpr int kAstmDirectLuminance = 142;

}  // namespace

void LightingPanel::setSpectralMode(quantiloom::SpectralMode mode) {
    m_previousSpectralMode = m_spectralMode;
    m_spectralMode = mode;
    updateIlluminantWidgets();
}

void LightingPanel::syncStrengthWidgets() {
    // "None" in a spectral mode: no spectrum is bound, so the sun and sky emit
    // nothing whatever their strengths say, and the fields show that rather
    // than numbers with no effect. They take no input there -- a strength
    // needs a spectrum to mean anything. The values behind them are left as
    // they are, because they are the RGB path's: a document's RGB sun comes
    // back when the mode does.
    const bool sunOff = m_spectralMode != quantiloom::SpectralMode::RGB &&
                        m_illuminantCombo->currentData().toString() == QLatin1String("none");
    const QSignalBlocker sun(m_sunIntensitySpin);
    const QSignalBlocker sky(m_skyIntensitySpin);
    m_sunIntensitySpin->setValue(sunOff ? 0.0 : static_cast<double>(m_sunIntensity));
    m_skyIntensitySpin->setValue(sunOff ? 0.0 : static_cast<double>(m_skyIntensity));
    m_sunIntensitySpin->setEnabled(!sunOff);
    m_skyIntensitySpin->setEnabled(!sunOff);
}

bool LightingPanel::illuminantKindOffered(const QString& kind) const {
    // The built-in spectra -- ASTM G-173 and the flat CIE E -- are listed only
    // where a spectrum is rendered. RGB reduces any spectrum to a colour, so
    // there they would be one more way to tint the sun. "None" and a file are
    // listed everywhere; "None" means the RGB radiance in RGB and the sun and
    // sky switched off in the spectral modes (switchSunAndSkyOff).
    if (kind == QLatin1String("astm") || kind == QLatin1String("equal_energy")) {
        return m_spectralMode != quantiloom::SpectralMode::RGB;
    }
    return true;
}

bool LightingPanel::sunOrSkyLit() const {
    return m_sunIntensity > 0.0f || m_skyIntensity > 0.0f;
}

void LightingPanel::switchSunAndSkyOff() {
    // "None" chosen in a spectral mode. The fields already read 0
    // (syncStrengthWidgets); this makes the values behind them agree, so the
    // sun stays off when the mode changes too -- it is the user turning it
    // off, not a document that never said. The core requires solar_lut for
    // any spectral scene whose sun or sky is non-zero, and zeroing both is
    // its own advice for a scene lit only by emissive materials. The chroma
    // is kept, so a strength typed later comes back in the colour it had.
    if (!sunOrSkyLit()) {
        return;
    }
    m_sunIntensity = 0.0f;
    m_sunRadiance = glm::vec3(0.0f);
    m_skyIntensity = 0.0f;
    m_skyRadiance = glm::vec3(0.0f);
    syncStrengthWidgets();
    updateEnvironmentDisplay();
    emitEdit();
}

bool LightingPanel::showsIlluminantKind(const QString& kind) const {
    // The entry in force stays listed even where the mode does not offer it.
    // Only a document can put one there -- env_grassland names ASTM in an RGB
    // scene -- and loading may not overrule a document, so the list says what
    // is set rather than pretending it is something else.
    return illuminantKindOffered(kind) ||
           kind == m_illuminantCombo->currentData().toString();
}

void LightingPanel::repickForSpectralMode() {
    const QString kind = m_illuminantCombo->currentData().toString();
    const bool rgb = m_spectralMode == quantiloom::SpectralMode::RGB;
    const bool fromRgb = m_previousSpectralMode == quantiloom::SpectralMode::RGB;

    // RGB normalises a file (recommendedNormalise), which says nothing about
    // the level it should have in a spectral mode -- that is the file's own
    // business. So the setting it had is put aside on the way into RGB and
    // given back on the way out; otherwise a round trip through RGB left an
    // absolute spectrum normalised, and the spectral frame 142x too dark.
    if (rgb && !fromRgb) {
        m_fileNormaliseBeforeRgb.reset();
        if (kind == QLatin1String("file")) {
            m_fileNormaliseBeforeRgb =
                std::make_pair(m_illuminant.path, m_illuminant.normaliseUnitLuminance);
        }
    }
    if (!rgb && fromRgb && kind == QLatin1String("file") && m_fileNormaliseBeforeRgb &&
        m_fileNormaliseBeforeRgb->first == m_illuminant.path) {
        const bool restored = m_fileNormaliseBeforeRgb->second;
        m_fileNormaliseBeforeRgb.reset();
        setNormalisation(restored);
        return;
    }

    if (rgb && !illuminantKindOffered(kind)) {
        // A built-in spectrum into RGB. The core derives the sun's RGB from
        // the spectrum it loads, and an absolute ASTM's is about 142x the
        // display range, so it is normalised first. Then the spectrum is let
        // go and the colour it gave is kept: "None" is the sun as RGB
        // radiance, which is all RGB renders anyway.
        repickNormalisation();
        m_illuminantCombo->setCurrentIndex(
            m_illuminantCombo->findData(QStringLiteral("none")));
        return;
    }
    if (!rgb && fromRgb && kind == QLatin1String("none") && sunOrSkyLit()) {
        // A lit sun with no spectrum -- RGB's "None" -- into a spectral mode,
        // which lights nothing without one: it gets the bundled sun. A sun
        // that was already off stays off, which is what "None" means here,
        // and so does "None" moving between two spectral modes.
        m_illuminantCombo->setCurrentIndex(
            m_illuminantCombo->findData(QStringLiteral("astm")));
        return;
    }
    repickNormalisation();
    // Where setCurrentIndex ran above, onIlluminantKindChanged re-picked the
    // normalisation for the new illuminant and emitted.
}

void LightingPanel::repickNormalisation() {
    // Only where there is a defensible answer, and only when it disagrees with
    // what is set -- a user who overrode the recommendation and changed nothing
    // since keeps their override.
    const std::optional<bool> wanted = recommendedNormalise();
    if (wanted.has_value()) {
        setNormalisation(*wanted);
    }
}

void LightingPanel::setNormalisation(bool normalise) {
    if (normalise == m_illuminant.normaliseUnitLuminance) {
        return;
    }
    m_illuminant.normaliseUnitLuminance = normalise;
    {
        const QSignalBlocker blocker(m_normaliseCheck);
        m_normaliseCheck->setChecked(normalise);
    }
    updateIlluminantWidgets();
    emit illuminantChanged(m_illuminant);
}

bool LightingPanel::spectralModeIsQuantitative() const {
    return m_spectralMode != quantiloom::SpectralMode::RGB &&
           !catalog::spectralModeIsPreviewOnly(m_spectralMode);
}

// The render mode decides this, because the two halves of the renderer consume
// the illuminant on completely different scales. RGB multiplies the curve's
// integrated colour in directly -- ASTM G-173's direct beam is linear sRGB
// (164.8, 138.6, 112.1), so an un-normalised RGB frame is white everywhere but
// the shadows. The spectral path integrates per nanometre and stores band
// radiance averaged over the band; the same scene and the same un-normalised
// spectrum land at about 0.05 in VIS_Fused, correctly exposed and carrying real
// W/m²/sr. Measured both ways on env_seaside: white against a readable image.
//
// So an absolute spectrum has to lose its scale to be a picture and has to keep
// it to be a measurement, and which of those is being asked for is exactly what
// the mode says.
//
// Empty wherever the panel cannot know: a user's own file in a spectral mode,
// whose level only the file knows, or an illuminant where the setting does
// nothing at all.
std::optional<bool> LightingPanel::recommendedNormalise() const {
    const QString kind = m_illuminantCombo->currentData().toString();

    // Nothing to normalise, and nothing normalising would do: CIE E is built at
    // unit luminance already (a flat spectrum of height 1/106.856), so the
    // division is by 1.0 exactly.
    if (kind == QLatin1String("none") || kind == QLatin1String("equal_energy")) {
        return std::nullopt;
    }
    // A file's absolute level is the file's business. It could be a relative
    // reference spectrum, which wants normalising, or a radiometric measurement,
    // which does not, and the panel cannot tell them apart without reading it --
    // which is the core's job, not this panel's. Except in RGB, which keeps
    // only the colour: there any level but unit luminance scales the frame,
    // whatever the file meant, and the box that could say otherwise is hidden.
    if (kind == QLatin1String("file")) {
        if (m_spectralMode == quantiloom::SpectralMode::RGB) {
            return true;
        }
        return std::nullopt;
    }
    // ASTM G-173, the one absolute spectrum on offer: keep the scale where the
    // mode reports radiance, lose it where the mode makes a picture.
    return !spectralModeIsQuantitative();
}

QStringList LightingPanel::illuminantNotices() const {
    const QString kind = m_illuminantCombo->currentData().toString();
    const bool normalising = m_normaliseCheck->isChecked();
    QStringList notices;

    // "None" in a spectral mode is the sun and sky switched off: no spectrum
    // is bound, the strengths read 0, and nothing below applies.
    if (m_spectralMode != quantiloom::SpectralMode::RGB && kind == QLatin1String("none")) {
        return notices;
    }

    // Does the spectrum reach the wavelengths being rendered? The core warns
    // about this at load time and the log is the only place it has ever been
    // visible. Only checkable for the bundled spectrum, whose span is known.
    if (kind == QLatin1String("astm")) {
        if (const auto band = quantiloom::GetFusedBandInfo(m_spectralMode);
            band && (kAstmMinNm > band->lambdaMinNm || kAstmMaxNm < band->lambdaMaxNm)) {
            notices << tr("ASTM G-173 spans %1–%2 nm but this mode renders %3–%4 nm. The "
                          "spectrum is held flat at its last value across the gap rather "
                          "than left dark, so the result will look reasonable and mean "
                          "nothing.")
                           .arg(static_cast<int>(kAstmMinNm))
                           .arg(static_cast<int>(kAstmMaxNm))
                           .arg(static_cast<int>(band->lambdaMinNm))
                           .arg(static_cast<int>(band->lambdaMaxNm));
        }
    }

    if (kind == QLatin1String("equal_energy")) {
        notices << tr("Equal energy is defined at unit luminance, so normalising it "
                      "divides by 1 and changes nothing.");
    }

    // The two ways to get an absolute spectrum's scale wrong, one per direction.
    if (kind == QLatin1String("astm") && spectralModeIsQuantitative() && normalising) {
        notices << tr("ASTM G-173 is an absolute spectrum — 900 W/m² direct, 1000 W/m² "
                      "global. Normalising divides both curves by the sun's luminance "
                      "(%1) and discards the absolute scale this mode exists to report. "
                      "The spectral path is already exposed for it; use display "
                      "enhancement if the viewport reads dark.")
                       .arg(kAstmDirectLuminance);
    } else if (kind == QLatin1String("astm") && !spectralModeIsQuantitative() &&
               !normalising) {
        notices << tr("This mode multiplies the illuminant's colour in directly, and "
                      "un-normalised ASTM G-173 is about %1× the display range — the "
                      "frame will be white almost everywhere. Normalise it here; the "
                      "absolute scale only means something in the quantitative spectral "
                      "modes.")
                       .arg(kAstmDirectLuminance);
    }

    return notices;
}

void LightingPanel::setIlluminant(const IlluminantChoice& choice) {
    const QSignalBlocker blockCombo(m_illuminantCombo);
    const QSignalBlocker blockNormalise(m_normaliseCheck);

    m_illuminant = choice;
    const int index = m_illuminantCombo->findData(choice.kind);
    m_illuminantCombo->setCurrentIndex(index >= 0 ? index : 0);
    m_normaliseCheck->setChecked(choice.normaliseUnitLuminance);
    updateIlluminantWidgets();
}

LightingPanel::IlluminantChoice LightingPanel::illuminant() const {
    return m_illuminant;
}

void LightingPanel::updateIlluminantWidgets() {
    const QString kind = m_illuminantCombo->currentData().toString();
    const bool isFile = (kind == QLatin1String("file"));

    m_illuminantBrowseButton->setVisible(isFile);
    m_illuminantPathLabel->setVisible(isFile);
    if (isFile) {
        m_illuminantPathLabel->setText(
            m_illuminant.path.isEmpty()
                ? tr("No spectrum chosen")
                : QFontMetrics(m_illuminantPathLabel->font())
                      .elidedText(m_illuminant.path, Qt::ElideMiddle,
                                  std::max(80, m_illuminantPathLabel->width())));
        m_illuminantPathLabel->setToolTip(m_illuminant.path);
    }
    // Normalisation is meaningless with no spectrum to normalise, and a no-op
    // for the one illuminant that is already at unit luminance. Both are left
    // switched off rather than hidden: the value still round-trips through the
    // configuration, and a control that vanished would look like it had been
    // decided rather than like it does not apply.
    m_normaliseCheck->setEnabled(kind != QLatin1String("none") &&
                                 kind != QLatin1String("equal_energy"));
    // RGB is the opposite case: it keeps only the illuminant's colour, so the
    // level is decided there (recommendedNormalise) and the box is hidden.
    // ASTM is the exception, and RGB only has it when a document asked for
    // it: its scale is the document's, and the notice below says to
    // normalise it here.
    m_normaliseCheck->setVisible(m_spectralMode != quantiloom::SpectralMode::RGB ||
                                 kind == QLatin1String("astm"));

    syncStrengthWidgets();

    // "None" is two different things, so it says which one is in force.
    m_illuminantCombo->setItemText(
        m_illuminantCombo->findData(QStringLiteral("none")),
        m_spectralMode == quantiloom::SpectralMode::RGB ? tr("None — RGB radiance only")
                                                        : tr("None — sun and sky off"));

    // The list follows the mode (illuminantKindOffered). Hidden rows leave the
    // popup, and disabled ones are skipped by the wheel and the arrow keys,
    // which would otherwise still step onto a hidden row.
    auto* view = qobject_cast<QListView*>(m_illuminantCombo->view());
    auto* model = qobject_cast<QStandardItemModel*>(m_illuminantCombo->model());
    for (int row = 0; row < m_illuminantCombo->count(); ++row) {
        const bool shown = showsIlluminantKind(m_illuminantCombo->itemData(row).toString());
        view->setRowHidden(row, !shown);
        model->item(row)->setEnabled(shown);
    }

    const QStringList notices = illuminantNotices();
    m_illuminantNotice->setVisible(!notices.isEmpty());
    m_illuminantNotice->setText(notices.join(QStringLiteral("\n\n")));
}

void LightingPanel::requestEnvironmentEnabled(bool enabled) {
    m_envEnabledCheck->setChecked(enabled);   // toggled() runs the handler
}

void LightingPanel::requestIlluminantKind(const QString& kind) {
    const int index = m_illuminantCombo->findData(kind);
    if (index < 0 || !showsIlluminantKind(kind)) {
        return;
    }
    if (index == m_illuminantCombo->currentIndex()) {
        // Picking the file entry again is a request for a different file.
        if (kind == QLatin1String("file")) {
            onBrowseIlluminant();
        }
        return;
    }
    m_illuminantCombo->setCurrentIndex(index);   // runs onIlluminantKindChanged()
    if (kind == QLatin1String("file") && m_illuminant.path.isEmpty()) {
        onBrowseIlluminant();
    }
}

void LightingPanel::onIlluminantKindChanged() {
    m_illuminant.kind = m_illuminantCombo->currentData().toString();

    // A change of illuminant re-picks the normalisation, for the same reason a
    // change of mode does: switching to ASTM in a quantitative mode should not
    // silently keep a normalisation that was right for the illuminant before
    // it. The user can still override it afterwards, and that override sticks.
    if (const std::optional<bool> wanted = recommendedNormalise(); wanted.has_value()) {
        m_illuminant.normaliseUnitLuminance = *wanted;
        const QSignalBlocker blocker(m_normaliseCheck);
        m_normaliseCheck->setChecked(*wanted);
    } else {
        m_illuminant.normaliseUnitLuminance = m_normaliseCheck->isChecked();
    }

    updateIlluminantWidgets();
    // A file chosen but not yet browsed for is not a change worth applying.
    if (m_illuminant.kind == QLatin1String("file") && m_illuminant.path.isEmpty()) {
        return;
    }
    emit illuminantChanged(m_illuminant);

    if (m_illuminant.kind == QLatin1String("none") &&
        m_spectralMode != quantiloom::SpectralMode::RGB) {
        switchSunAndSkyOff();
    }
}

QString LightingPanel::illuminantKindText(const QString& kind) const {
    return m_illuminantCombo->itemText(m_illuminantCombo->findData(kind));
}

void LightingPanel::onNormaliseToggled() {
    // Deliberately does not consult recommendedNormalise(): this *is* the user
    // disagreeing with it, and a recommendation that reasserted itself here
    // would be a checkbox that could not be unticked.
    m_illuminant.normaliseUnitLuminance = m_normaliseCheck->isChecked();
    updateIlluminantWidgets();
    if (m_illuminant.kind == QLatin1String("file") && m_illuminant.path.isEmpty()) {
        return;
    }
    emit illuminantChanged(m_illuminant);
}

void LightingPanel::onBrowseIlluminant() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose an Illuminant Spectrum"), QString(),
        tr("Spectrum files (*.csv *.txt *.dat);;All Files (*)"));
    if (path.isEmpty()) {
        return;
    }
    m_illuminant.path = path;
    updateIlluminantWidgets();
    emit illuminantChanged(m_illuminant);
}

void LightingPanel::retranslateUi() {
    m_sunDirGroup->setTitle(tr("Sun Direction"));
    m_azimuthCaption->setText(tr("Azimuth:"));
    m_elevationCaption->setText(tr("Elevation:"));
    const QString azimuthTip =
        tr("Compass bearing of the sun: 0° is north (+Z), 90° is east (+X), "
           "180° south, 270° west.");
    m_azimuthCaption->setToolTip(azimuthTip);
    m_azimuthSlider->setToolTip(azimuthTip);
    m_azimuthSpin->setToolTip(azimuthTip);
    const QString elevationTip =
        tr("Height of the sun above the horizon: 0° on the horizon, 90° overhead.");
    m_elevationCaption->setToolTip(elevationTip);
    m_elevationSlider->setToolTip(elevationTip);
    m_elevationSpin->setToolTip(elevationTip);

    m_illuminantGroup->setTitle(tr("Illuminant"));
    // Item 0, "None", depends on the mode and is labelled by
    // updateIlluminantWidgets() below.
    m_illuminantCombo->setItemText(1, tr("Equal energy (CIE E, flat spectrum)"));
    m_illuminantCombo->setItemText(2, tr("ASTM G-173 (bundled)"));
    m_illuminantCombo->setItemText(3, tr("From file..."));
    m_illuminantCombo->setToolTip(
        tr("The sun's spectrum, as distinct from the RGB radiance above. The "
           "spectral modes light the scene only through one; there, None switches "
           "the sun and sky off, for a scene lit by its own emissive materials."));
    m_illuminantBrowseButton->setText(tr("Choose Spectrum..."));
    m_normaliseCheck->setText(tr("Normalise to unit luminance"));
    m_normaliseCheck->setToolTip(
        tr("Published reference spectra are relative, so their absolute level is "
           "arbitrary. Both curves scale by the sun's luminance, which keeps the "
           "sun-to-sky ratio the measurement actually recorded.\n\n"
           "Whether it is right depends on the render mode. RGB and the preview bands "
           "multiply the illuminant's colour in directly and need an absolute spectrum "
           "normalised or the frame is white; the quantitative spectral modes integrate "
           "per wavelength, are already exposed for it, and are the reason to keep the "
           "absolute scale. It is picked for you when the mode or the illuminant "
           "changes, and you can still override it."));
    updateIlluminantWidgets();

    m_radianceGroup->setTitle(tr("Sun and Sky Strength"));
    m_sunCaption->setText(tr("Sun:"));
    m_skyCaption->setText(tr("Sky:"));
    // Suffixes are part of the value display, so they are reapplied here too.
    // The sun is a delta light, so what it carries is an irradiance, not the
    // disc's radiance -- the SDK's LightingParams documents the field so.
    m_sunIntensitySpin->setSuffix(tr(" W/m²"));
    m_skyIntensitySpin->setSuffix(tr(" W/m²/sr"));
    const QString sunTip =
        tr("Irradiance on a surface facing the sun, per RGB channel. The sun is a "
           "point light, so this is not the radiance of its disc, which would be "
           "about 15,000 times larger.");
    m_sunCaption->setToolTip(sunTip);
    m_sunIntensitySpin->setToolTip(sunTip);
    const QString skyTip = tr("Radiance of the uniform sky, per RGB channel.");
    m_skyCaption->setToolTip(skyTip);
    m_skyIntensitySpin->setToolTip(skyTip);

    m_envGroup->setTitle(tr("Environment Map (IBL)"));
    m_envEnabledCheck->setText(tr("Light the scene from the environment map"));
    m_envEnabledCheck->setToolTip(
        tr("Off means the map contributes no light at all — not that it is "
           "replaced by another sky. What a ray sees when it misses the scene "
           "is the sky radiance above, either way."));
    m_envBrowseButton->setText(tr("Browse…"));
    m_envClearButton->setText(tr("Clear"));


    updateEnvironmentDisplay();
}

void LightingPanel::setEnvironmentMap(const QString& path, bool enabled) {
    m_environmentMapPath = path;
    m_environmentEnabled = enabled;
    {
        const QSignalBlocker block(m_envEnabledCheck);
        m_envEnabledCheck->setChecked(enabled);
    }
    updateEnvironmentDisplay();
}

void LightingPanel::resizeEvent(QResizeEvent* event) {
    PanelBase::resizeEvent(event);
    updateEnvironmentDisplay();
}

void LightingPanel::updateEnvironmentDisplay() {
    if (!m_envPathLabel) {
        return;
    }

    if (m_environmentMapPath.isEmpty()) {
        m_envPathLabel->setText(tr("No map — lighting from the built-in sky"));
        m_envPathLabel->setToolTip(QString());
        m_envClearButton->setEnabled(false);
    } else {
        const QFontMetrics metrics(m_envPathLabel->font());
        const int available = std::max(80, m_envPathLabel->width());
        m_envPathLabel->setText(
            metrics.elidedText(m_environmentMapPath, Qt::ElideMiddle, available));
        m_envPathLabel->setToolTip(m_environmentMapPath);
        m_envClearButton->setEnabled(true);
    }

    // Only when a map is actually lighting the scene *and* an analytic source
    // is too. Either one alone is a legitimate way to light a render.
    const bool analyticLit = m_sunIntensity > 0.0f || m_skyIntensity > 0.0f;
    const bool doubleCounted =
        m_environmentEnabled && !m_environmentMapPath.isEmpty() && analyticLit;
    m_doubleCountNotice->setVisible(doubleCounted);
    if (doubleCounted) {
        m_doubleCountNotice->setText(
            tr("Preview only — not quantitative: an environment map and an "
               "analytic sun or sky are both lighting the scene, so the same "
               "illumination is counted twice. An HDRI sky already contains its "
               "own sun, and nothing aligns the two directions — expect two "
               "specular highlights in different places. Set sun and sky to 0 to "
               "light from the map alone, or turn the map off."));
    }
}

void LightingPanel::onEnvironmentEnabledToggled(bool enabled) {
    m_environmentEnabled = enabled;
    updateEnvironmentDisplay();
    emit environmentMapChanged(m_environmentMapPath, m_environmentEnabled);
    emitChanges();
}

void LightingPanel::onBrowseEnvironmentMap() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose Environment Map"),
        QFileInfo(m_environmentMapPath).absolutePath(),
        tr("Environment Maps (*.exr *.hdr *.png *.jpg *.jpeg);;All Files (*)"));
    if (path.isEmpty()) {
        return;
    }

    m_environmentMapPath = path;
    // Choosing one is asking to light with it; a browse that left the map off
    // would look like the dialog did nothing.
    if (!m_environmentEnabled) {
        m_environmentEnabled = true;
        const QSignalBlocker block(m_envEnabledCheck);
        m_envEnabledCheck->setChecked(true);
    }
    updateEnvironmentDisplay();
    emit environmentMapChanged(m_environmentMapPath, m_environmentEnabled);
    emitChanges();
}

void LightingPanel::onClearEnvironmentMap() {
    m_environmentMapPath.clear();
    updateEnvironmentDisplay();
    emit environmentMapChanged(m_environmentMapPath, m_environmentEnabled);
    emitChanges();
}

void LightingPanel::setLightingParams(const quantiloom::LightingParams& params) {
    // sunDirection points FROM surface TO sun
    const glm::vec3& dir = params.sunDirection;

    // Held at full precision: a sun below the horizon is a legitimate document
    // value and must survive the next edit. Only the widgets, whose range is
    // 0..90, clamp what they show (syncAngleWidgets).
    m_sunElevation = std::asin(dir.y) * 180.0f / static_cast<float>(M_PI);

    m_sunAzimuth = std::atan2(dir.x, dir.z) * 180.0f / static_cast<float>(M_PI);
    if (m_sunAzimuth < 0) m_sunAzimuth += 360.0f;

    // The spin box shows the mean of the RGB triple; the triple's shape is
    // kept as a chroma normalised to mean 1, so editing the level scales the
    // colour the config gave instead of replacing it with grey or a tint of
    // this panel's own choosing. A zero triple carries no chroma, and keeps
    // the one already held.
    m_sunRadiance = params.sunRadiance_rgb;
    m_sunIntensity = (m_sunRadiance.r + m_sunRadiance.g + m_sunRadiance.b) / 3.0f;
    if (m_sunIntensity > 0.0f) {
        m_sunChroma = m_sunRadiance / m_sunIntensity;
    }

    m_skyRadiance = params.skyRadiance_rgb;
    m_skyIntensity = (m_skyRadiance.r + m_skyRadiance.g + m_skyRadiance.b) / 3.0f;
    if (m_skyIntensity > 0.0f) {
        m_skyChroma = m_skyRadiance / m_skyIntensity;
    }

    m_chromaR_correction = params.chromaR_correction;
    m_chromaB_correction = params.chromaB_correction;
    m_enableShadowRays = (params.enableShadowRays != 0);
    m_environmentEnabled = (params.enableEnvironmentMap != 0);

    // Update UI (block signals to avoid feedback loop)
    {
        const QSignalBlocker envCheck(m_envEnabledCheck);
        m_envEnabledCheck->setChecked(m_environmentEnabled);
    }
    syncAngleWidgets();
    syncStrengthWidgets();


    // Radiance and the enable flag both just changed, and the notice reads both.
    updateEnvironmentDisplay();
}

void LightingPanel::syncAngleWidgets() {
    const QSignalBlocker azimuthSlider(m_azimuthSlider);
    const QSignalBlocker azimuthSpin(m_azimuthSpin);
    const QSignalBlocker elevationSlider(m_elevationSlider);
    const QSignalBlocker elevationSpin(m_elevationSpin);
    m_azimuthSlider->setValue(static_cast<int>(std::lround(m_sunAzimuth)));
    m_azimuthSpin->setValue(static_cast<double>(m_sunAzimuth));
    const float shownElevation = std::clamp(m_sunElevation, 0.0f, 90.0f);
    m_elevationSlider->setValue(static_cast<int>(std::lround(shownElevation)));
    m_elevationSpin->setValue(static_cast<double>(shownElevation));
}

void LightingPanel::emitEdit() {
    // A slider drag is bracketed by press and release, which the shell turns
    // into one undo entry. Anything else -- a typed value, a spin arrow, a
    // click on the groove, a key -- is a whole edit by itself, and gets the
    // same bracket here so it is undoable too.
    if (m_azimuthSlider->isSliderDown() || m_elevationSlider->isSliderDown()) {
        emitChanges();
        return;
    }
    emit editGestureStarted();
    emitChanges();
    emit editGestureFinished();
}

void LightingPanel::onSunAzimuthChanged(int value) {
    m_sunAzimuth = static_cast<float>(value);
    const QSignalBlocker block(m_azimuthSpin);
    m_azimuthSpin->setValue(value);
    emitEdit();
}

void LightingPanel::onSunElevationChanged(int value) {
    m_sunElevation = static_cast<float>(value);
    const QSignalBlocker block(m_elevationSpin);
    m_elevationSpin->setValue(value);
    emitEdit();
}

void LightingPanel::onSunAzimuthSpinChanged(double value) {
    m_sunAzimuth = static_cast<float>(value);
    const QSignalBlocker block(m_azimuthSlider);
    m_azimuthSlider->setValue(static_cast<int>(std::lround(value)));
    emitEdit();
}

void LightingPanel::onSunElevationSpinChanged(double value) {
    m_sunElevation = static_cast<float>(value);
    const QSignalBlocker block(m_elevationSlider);
    m_elevationSlider->setValue(static_cast<int>(std::lround(value)));
    emitEdit();
}

void LightingPanel::onSunIntensityChanged(double value) {
    m_sunIntensity = static_cast<float>(value);
    m_sunRadiance = m_sunChroma * m_sunIntensity;
    // The double-count notice depends on this being non-zero, so zeroing the
    // sun to light from the map alone has to make the notice go away.
    updateEnvironmentDisplay();
    emitEdit();
}

void LightingPanel::onSkyIntensityChanged(double value) {
    m_skyIntensity = static_cast<float>(value);
    m_skyRadiance = m_skyChroma * m_skyIntensity;
    updateEnvironmentDisplay();
    emitEdit();
}

void LightingPanel::emitChanges() {
    quantiloom::LightingParams params = quantiloom::CreateDefaultLightingParams();
    params.sunDirection = sunDirectionFromAngles(m_sunAzimuth, m_sunElevation);
    params.sunRadiance_spectral = m_sunIntensity;
    params.sunRadiance_rgb = m_sunRadiance;
    params.skyRadiance_spectral = m_skyIntensity;
    params.skyRadiance_rgb = m_skyRadiance;
    params.chromaR_correction = m_chromaR_correction;
    params.chromaB_correction = m_chromaB_correction;
    params.enableShadowRays = m_enableShadowRays ? 1u : 0u;
    params.enableEnvironmentMap = m_environmentEnabled ? 1u : 0u;

    // transmittance and atmosphereTemperature_K are left at their defaults on
    // purpose: the atmosphere panel owns them, and the shell merges the two
    // halves before anything reaches the renderer.
    emit lightingChanged(params);
}
