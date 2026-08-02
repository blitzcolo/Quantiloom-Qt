/**
 * @file LightingPanel.cpp
 * @brief Sun/sky lighting parameter editor implementation
 */

#include "LightingPanel.hpp"

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
#include <QPushButton>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <algorithm>
#include <cmath>

#include <renderer/LightingParams.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

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
    m_azimuthLabel = new QLabel(QStringLiteral("180°"));
    m_azimuthLabel->setMinimumWidth(45);
    connect(m_azimuthSlider, &QSlider::valueChanged, this, &LightingPanel::onSunAzimuthChanged);
    connect(m_azimuthSlider, &QSlider::sliderPressed, this, &LightingPanel::editGestureStarted);
    connect(m_azimuthSlider, &QSlider::sliderReleased, this, &LightingPanel::editGestureFinished);
    azimuthRow->addWidget(m_azimuthSlider);
    azimuthRow->addWidget(m_azimuthLabel);
    m_azimuthCaption = new QLabel(m_sunDirGroup);
    sunDirLayout->addRow(m_azimuthCaption, azimuthRow);

    auto* elevationRow = new QHBoxLayout();
    m_elevationSlider = new QSlider(Qt::Horizontal);
    m_elevationSlider->setRange(0, 90);
    m_elevationSlider->setValue(45);
    m_elevationLabel = new QLabel(QStringLiteral("45°"));
    m_elevationLabel->setMinimumWidth(45);
    connect(m_elevationSlider, &QSlider::valueChanged, this, &LightingPanel::onSunElevationChanged);
    connect(m_elevationSlider, &QSlider::sliderPressed, this, &LightingPanel::editGestureStarted);
    connect(m_elevationSlider, &QSlider::sliderReleased, this, &LightingPanel::editGestureFinished);
    elevationRow->addWidget(m_elevationSlider);
    elevationRow->addWidget(m_elevationLabel);
    m_elevationCaption = new QLabel(m_sunDirGroup);
    sunDirLayout->addRow(m_elevationCaption, elevationRow);

    mainLayout->addWidget(m_sunDirGroup);

    // Radiance
    m_radianceGroup = new QGroupBox(this);
    auto* radianceLayout = new QFormLayout(m_radianceGroup);

    m_sunIntensitySpin = new QDoubleSpinBox();
    m_sunIntensitySpin->setRange(0.0, 100.0);
    m_sunIntensitySpin->setSingleStep(0.1);
    m_sunIntensitySpin->setValue(1.0);
    connect(m_sunIntensitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LightingPanel::onSunIntensityChanged);
    m_sunCaption = new QLabel(m_radianceGroup);
    radianceLayout->addRow(m_sunCaption, m_sunIntensitySpin);

    m_skyIntensitySpin = new QDoubleSpinBox();
    m_skyIntensitySpin->setRange(0.0, 10.0);
    m_skyIntensitySpin->setSingleStep(0.01);
    m_skyIntensitySpin->setValue(0.1);
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

    mainLayout->addWidget(m_envGroup);

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
            this, &LightingPanel::onIlluminantChanged);
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
            this, &LightingPanel::onIlluminantChanged);
    illuminantLayout->addWidget(m_normaliseCheck);

    m_illuminantNotice = new QLabel(m_illuminantGroup);
    m_illuminantNotice->setWordWrap(true);
    m_illuminantNotice->setVisible(false);
    bindStyle([this] { uistyle::applyNoticeStyle(m_illuminantNotice); });
    illuminantLayout->addWidget(m_illuminantNotice);

    mainLayout->addWidget(m_illuminantGroup);
    mainLayout->addStretch();
}

void LightingPanel::setSpectralModeIsQuantitative(bool quantitative) {
    m_spectralModeQuantitative = quantitative;
    updateIlluminantWidgets();
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
    // Normalisation is meaningless with no spectrum to normalise.
    m_normaliseCheck->setEnabled(kind != QLatin1String("none"));

    // The core's own diagnostic, said before the render rather than after:
    // a spectral mode with no illuminant is the black-frame case.
    const bool missing = m_spectralModeQuantitative && kind == QLatin1String("none");
    m_illuminantNotice->setVisible(missing);
    if (missing) {
        m_illuminantNotice->setText(
            tr("This spectral mode needs an illuminant spectrum. Without one the "
               "render is black — the renderer will not substitute a standard "
               "spectrum, because a scene that acquired one silently would report "
               "radiance nobody asked for."));
    }
}

void LightingPanel::onIlluminantChanged() {
    m_illuminant.kind = m_illuminantCombo->currentData().toString();
    m_illuminant.normaliseUnitLuminance = m_normaliseCheck->isChecked();
    updateIlluminantWidgets();
    // A file chosen but not yet browsed for is not a change worth applying.
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

    m_illuminantGroup->setTitle(tr("Illuminant"));
    m_illuminantCombo->setItemText(0, tr("None — RGB radiance only"));
    m_illuminantCombo->setItemText(1, tr("Equal energy (CIE E)"));
    m_illuminantCombo->setItemText(2, tr("ASTM G-173 (bundled)"));
    m_illuminantCombo->setItemText(3, tr("From file..."));
    m_illuminantCombo->setToolTip(
        tr("The sun's spectrum, as distinct from the RGB radiance above. The "
           "quantitative spectral modes need one to render anything at all."));
    m_illuminantBrowseButton->setText(tr("Choose Spectrum..."));
    m_normaliseCheck->setText(tr("Normalise to unit luminance"));
    m_normaliseCheck->setToolTip(
        tr("Published reference spectra are relative, so their absolute level is "
           "arbitrary. Both curves scale by the sun's luminance, which keeps the "
           "sun-to-sky ratio the measurement actually recorded."));
    updateIlluminantWidgets();

    m_radianceGroup->setTitle(tr("Radiance"));
    m_sunCaption->setText(tr("Sun:"));
    m_skyCaption->setText(tr("Sky:"));
    // Suffixes are part of the value display, so they are reapplied here too.
    m_sunIntensitySpin->setSuffix(tr(" W/m²/sr"));
    m_skyIntensitySpin->setSuffix(tr(" W/m²/sr"));

    m_envGroup->setTitle(tr("Environment Map (IBL)"));
    m_envEnabledCheck->setText(tr("Light the scene from the environment map"));
    m_envEnabledCheck->setToolTip(
        tr("Off means the map contributes no light at all — not that it is "
           "replaced by another sky. What a ray sees when it misses the scene "
           "is the sky radiance above, either way."));
    m_envBrowseButton->setText(tr("Browse…"));
    m_envClearButton->setText(tr("Clear"));

    m_azimuthLabel->setText(tr("%1°").arg(static_cast<int>(m_sunAzimuth)));
    m_elevationLabel->setText(tr("%1°").arg(static_cast<int>(m_sunElevation)));

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

    m_sunElevation = std::asin(dir.y) * 180.0f / static_cast<float>(M_PI);
    m_sunElevation = std::max(0.0f, std::min(90.0f, m_sunElevation));

    m_sunAzimuth = std::atan2(dir.x, dir.z) * 180.0f / static_cast<float>(M_PI);
    if (m_sunAzimuth < 0) m_sunAzimuth += 360.0f;

    m_sunRadiance = params.sunRadiance_rgb;
    m_sunIntensity = (m_sunRadiance.r + m_sunRadiance.g + m_sunRadiance.b) / 3.0f;

    m_skyRadiance = params.skyRadiance_rgb;
    m_skyIntensity = (m_skyRadiance.r + m_skyRadiance.g + m_skyRadiance.b) / 3.0f;

    m_chromaR_correction = params.chromaR_correction;
    m_chromaB_correction = params.chromaB_correction;
    m_enableShadowRays = (params.enableShadowRays != 0);
    m_environmentEnabled = (params.enableEnvironmentMap != 0);

    // Update UI (block signals to avoid feedback loop)
    {
        const QSignalBlocker azimuth(m_azimuthSlider);
        m_azimuthSlider->setValue(static_cast<int>(m_sunAzimuth));
    }
    {
        const QSignalBlocker elevation(m_elevationSlider);
        m_elevationSlider->setValue(static_cast<int>(m_sunElevation));
    }
    {
        const QSignalBlocker sun(m_sunIntensitySpin);
        m_sunIntensitySpin->setValue(m_sunIntensity);
    }
    {
        const QSignalBlocker sky(m_skyIntensitySpin);
        m_skyIntensitySpin->setValue(m_skyIntensity);
    }

    m_azimuthLabel->setText(tr("%1°").arg(static_cast<int>(m_sunAzimuth)));
    m_elevationLabel->setText(tr("%1°").arg(static_cast<int>(m_sunElevation)));

    // Radiance and the enable flag both just changed, and the notice reads both.
    updateEnvironmentDisplay();
}

void LightingPanel::onSunAzimuthChanged(int value) {
    m_sunAzimuth = static_cast<float>(value);
    m_azimuthLabel->setText(tr("%1°").arg(value));
    emitChanges();
}

void LightingPanel::onSunElevationChanged(int value) {
    m_sunElevation = static_cast<float>(value);
    m_elevationLabel->setText(tr("%1°").arg(value));
    emitChanges();
}

void LightingPanel::onSunIntensityChanged(double value) {
    m_sunIntensity = static_cast<float>(value);
    m_sunRadiance = glm::vec3(m_sunIntensity);
    // The double-count notice depends on this being non-zero, so zeroing the
    // sun to light from the map alone has to make the notice go away.
    updateEnvironmentDisplay();
    emitChanges();
}

void LightingPanel::onSkyIntensityChanged(double value) {
    m_skyIntensity = static_cast<float>(value);
    // Keep sky color tint (blue-ish)
    m_skyRadiance = glm::vec3(m_skyIntensity * 1.0f, m_skyIntensity * 1.5f, m_skyIntensity * 2.0f);
    updateEnvironmentDisplay();
    emitChanges();
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
