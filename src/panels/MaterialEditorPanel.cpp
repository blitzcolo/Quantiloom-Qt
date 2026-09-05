/**
 * @file MaterialEditorPanel.cpp
 * @brief PBR material property editor implementation
 */

#include "MaterialEditorPanel.hpp"

#include "../ui/UiStyle.hpp"
#include "../ui/theme/ThemeManager.hpp"
#include "../ui/CollapsibleGroupBox.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QDoubleSpinBox>
#include <cmath>
#include <QPushButton>
#include <QColorDialog>
#include <QFormLayout>
#include <QMessageBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QComboBox>

#include <scene/Material.hpp>
#include <io/SpectralIO.hpp>

namespace {
/// This editor writes exactly two sample points per IR curve (one at MWIR, one
/// at LWIR). Anything longer came from the spectral material generator.
constexpr size_t kSimpleCurvePoints = 2;
} // namespace

MaterialEditorPanel::MaterialEditorPanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());
    setupUi();
}

QString MaterialEditorPanel::panelTitle() const {
    return tr("Material");
}

void MaterialEditorPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // Material name
    m_materialName = new QLabel(this);
    bindStyle([this] { uistyle::applyHeadingStyle(m_materialName); });
    bindText([this] {
        if (m_currentIndex < 0) {
            m_materialName->setText(tr("No material selected"));
        }
    });
    mainLayout->addWidget(m_materialName);

    // Base Color group
    auto* colorGroup = new QGroupBox(this);
    bindText([colorGroup] { colorGroup->setTitle(tr("Base Color")); });
    auto* colorLayout = new QHBoxLayout(colorGroup);
    m_baseColorBtn = new QPushButton();
    m_baseColorBtn->setFixedSize(80, 30);
    m_baseColorBtn->setStyleSheet(QStringLiteral("background-color: white;"));
    connect(m_baseColorBtn, &QPushButton::clicked, this, &MaterialEditorPanel::onBaseColorClicked);
    colorLayout->addWidget(m_baseColorBtn);
    colorLayout->addStretch();
    mainLayout->addWidget(colorGroup);

    // Metallic-Roughness group
    auto* pbrGroup = new QGroupBox(this);
    bindText([pbrGroup] { pbrGroup->setTitle(tr("PBR Properties")); });
    auto* pbrLayout = new QFormLayout(pbrGroup);

    // Slider plus spinbox, the pattern SpectralConfigPanel's wavelength row
    // established: the slider is for exploring, the spinbox for saying 0.35
    // exactly. A read-only label could do neither.
    auto* metallicRow = new QHBoxLayout();
    m_metallicSlider = new QSlider(Qt::Horizontal);
    m_metallicSlider->setRange(0, 100);
    m_metallicSlider->setValue(0);
    m_metallicSpin = new QDoubleSpinBox();
    m_metallicSpin->setRange(0.0, 1.0);
    m_metallicSpin->setSingleStep(0.01);
    m_metallicSpin->setDecimals(2);
    m_metallicSpin->setValue(0.0);
    connect(m_metallicSlider, &QSlider::valueChanged, this, &MaterialEditorPanel::onMetallicChanged);
    connect(m_metallicSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MaterialEditorPanel::onMetallicSpinChanged);
    metallicRow->addWidget(m_metallicSlider, 1);
    metallicRow->addWidget(m_metallicSpin);
    auto* metallicCaption = new QLabel(pbrGroup);
    bindText([metallicCaption] { metallicCaption->setText(tr("Metallic:")); });
    pbrLayout->addRow(metallicCaption, metallicRow);

    auto* roughnessRow = new QHBoxLayout();
    m_roughnessSlider = new QSlider(Qt::Horizontal);
    m_roughnessSlider->setRange(0, 100);
    m_roughnessSlider->setValue(100);
    m_roughnessSpin = new QDoubleSpinBox();
    m_roughnessSpin->setRange(0.0, 1.0);
    m_roughnessSpin->setSingleStep(0.01);
    m_roughnessSpin->setDecimals(2);
    m_roughnessSpin->setValue(1.0);
    connect(m_roughnessSlider, &QSlider::valueChanged, this, &MaterialEditorPanel::onRoughnessChanged);
    connect(m_roughnessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MaterialEditorPanel::onRoughnessSpinChanged);
    roughnessRow->addWidget(m_roughnessSlider, 1);
    roughnessRow->addWidget(m_roughnessSpin);
    auto* roughnessCaption = new QLabel(pbrGroup);
    bindText([roughnessCaption] { roughnessCaption->setText(tr("Roughness:")); });
    pbrLayout->addRow(roughnessCaption, roughnessRow);

    mainLayout->addWidget(pbrGroup);

    // Emissive group
    auto* emissiveGroup = new QGroupBox(this);
    bindText([emissiveGroup] { emissiveGroup->setTitle(tr("Emissive")); });
    auto* emissiveOuter = new QVBoxLayout(emissiveGroup);
    auto* emissiveRow = new QWidget(emissiveGroup);
    auto* emissiveLayout = new QHBoxLayout(emissiveRow);
    emissiveLayout->setContentsMargins(0, 0, 0, 0);
    emissiveOuter->addWidget(emissiveRow);

    // The ceiling is high because emissiveFactor is an HDR radiance scale, not
    // a colour. A glTF material carrying KHR_materials_emissive_strength has
    // the strength folded into these three numbers by the loader, so a sun
    // panel authored at 0.9 x 10000 arrives here as 9000. Loading is
    // signal-blocked, so a value above the ceiling would not corrupt the
    // material on the way in -- it would show as the clamp, and the next edit
    // of any one channel would write that clamp back over the other two.
    auto createSpinBox = [this]() {
        auto* spin = new QDoubleSpinBox();
        spin->setRange(0.0, 1.0e6);
        spin->setSingleStep(0.1);
        spin->setDecimals(2);
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &MaterialEditorPanel::onEmissiveChanged);
        return spin;
    };

    // R/G/B are channel symbols rather than English words, so they are kept
    // verbatim in every locale -- a decision recorded in the glossary, not an
    // oversight. They used to bypass tr() with no explanation either way.
    emissiveLayout->addWidget(new QLabel(QStringLiteral("R:")));
    m_emissiveR = createSpinBox();
    emissiveLayout->addWidget(m_emissiveR);

    emissiveLayout->addWidget(new QLabel(QStringLiteral("G:")));
    m_emissiveG = createSpinBox();
    emissiveLayout->addWidget(m_emissiveG);

    emissiveLayout->addWidget(new QLabel(QStringLiteral("B:")));
    m_emissiveB = createSpinBox();
    emissiveLayout->addWidget(m_emissiveB);

    // The spectrum this surface emits, which the RGB above cannot express.
    // Populated from the core's own list so a build of Studio never offers a
    // token the renderer would reject, and never withholds one it would take.
    auto* spectrumForm = new QFormLayout();
    spectrumForm->setContentsMargins(0, 0, 0, 0);

    m_emissiveCurve = new QComboBox();
    m_emissiveCurve->addItem(QString(), QString());  // text set by bindText below
    for (const auto& info : quantiloom::SpectralIO::BuiltinEmissionSpectra()) {
        const auto token = QString::fromStdString(info.token);
        m_emissiveCurve->addItem(
            QStringLiteral("%1 — %2").arg(token,
                                          QString::fromStdString(info.description)),
            token);
    }
    connect(m_emissiveCurve, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MaterialEditorPanel::onEmissiveCurveChanged);

    m_emissiveScale = new QComboBox();
    m_emissiveScale->addItem(QString(), QStringLiteral("match_luminance"));
    m_emissiveScale->addItem(QString(), QStringLiteral("absolute"));
    connect(m_emissiveScale, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MaterialEditorPanel::onEmissiveCurveChanged);

    auto* spectrumCaption = new QLabel();
    auto* scaleCaption = new QLabel();
    m_emissiveCurveNote = new QLabel();
    m_emissiveCurveNote->setWordWrap(true);
    m_emissiveCurveNote->setEnabled(false);

    bindText([this, spectrumCaption, scaleCaption] {
        spectrumCaption->setText(tr("Spectrum:"));
        scaleCaption->setText(tr("Level:"));
        m_emissiveCurve->setItemText(0, tr("None — expand the RGB through D65"));
        m_emissiveScale->setItemText(0, tr("Match the RGB's luminance"));
        m_emissiveScale->setItemText(1, tr("Absolute (W·m⁻²·sr⁻¹·nm⁻¹)"));
        m_emissiveCurve->setToolTip(
            tr("An RGB triple is not a lamp. Without a spectrum the renderer expands "
               "the colour above through D65 — a convention, defined only over "
               "380–780 nm, that nothing measured. The infrared bands read no "
               "emissive RGB at all, so a light is invisible to them until a "
               "spectrum is bound here."));
        m_emissiveScale->setToolTip(
            tr("Every built-in but the blackbody family is a relative distribution "
               "with no absolute level, so by default the spectrum supplies the "
               "shape and the RGB above supplies the brightness — swapping lamps "
               "then recolours the scene without re-exposing it. Choose Absolute "
               "for a calibrated measurement, where the level is the datum. "
               "Absolute is required outside the visible: luminance is a property "
               "of the CIE observer, which sees nothing at 10 µm."));
        updateEmissiveCurveNote();
    });

    spectrumForm->addRow(spectrumCaption, m_emissiveCurve);
    spectrumForm->addRow(scaleCaption, m_emissiveScale);
    spectrumForm->addRow(m_emissiveCurveNote);
    emissiveOuter->addLayout(spectrumForm);

    mainLayout->addWidget(emissiveGroup);

    // IR Properties group (for MWIR/LWIR modes)
    m_irGroup = new QGroupBox(this);
    bindText([this] { m_irGroup->setTitle(tr("IR Properties (Thermal)")); });
    auto* irLayout = new QFormLayout(m_irGroup);

    m_spectralCurveNotice = new QLabel(m_irGroup);
    bindStyle([this] { uistyle::applyNoticeStyle(m_spectralCurveNotice); });
    m_spectralCurveNotice->setVisible(false);
    irLayout->addRow(m_spectralCurveNotice);

    m_irEmissivitySpin = new QDoubleSpinBox();
    m_irEmissivitySpin->setRange(0.0, 1.0);
    m_irEmissivitySpin->setSingleStep(0.01);
    m_irEmissivitySpin->setDecimals(3);
    connect(m_irEmissivitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MaterialEditorPanel::onIRPropertyChanged);
    auto* emissivityCaption = new QLabel(m_irGroup);
    bindText([this, emissivityCaption] {
        emissivityCaption->setText(tr("Emissivity:"));
        m_irEmissivitySpin->setToolTip(
            tr("Fraction of blackbody radiation emitted (0 = reflective, 1 = perfect emitter)"));
    });
    irLayout->addRow(emissivityCaption, m_irEmissivitySpin);

    m_irTransmittanceSpin = new QDoubleSpinBox();
    m_irTransmittanceSpin->setRange(0.0, 1.0);
    m_irTransmittanceSpin->setSingleStep(0.01);
    m_irTransmittanceSpin->setDecimals(3);
    connect(m_irTransmittanceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MaterialEditorPanel::onIRPropertyChanged);
    auto* transmittanceCaption = new QLabel(m_irGroup);
    bindText([this, transmittanceCaption] {
        transmittanceCaption->setText(tr("Transmittance:"));
        m_irTransmittanceSpin->setToolTip(
            tr("Fraction of radiation transmitted through the material (0 = opaque)"));
    });
    irLayout->addRow(transmittanceCaption, m_irTransmittanceSpin);

    m_irTemperatureSpin = new QDoubleSpinBox();
    m_irTemperatureSpin->setRange(0.0, 2000.0);
    m_irTemperatureSpin->setSingleStep(10.0);
    m_irTemperatureSpin->setDecimals(1);
    connect(m_irTemperatureSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MaterialEditorPanel::onIRPropertyChanged);
    auto* temperatureCaption = new QLabel(m_irGroup);
    bindText([this, temperatureCaption] {
        // Qualified: five panels used to label five different physical
        // quantities "Temperature".
        temperatureCaption->setText(tr("Object temperature:"));
        m_irTemperatureSpin->setSuffix(tr(" K"));
        m_irTemperatureSpin->setToolTip(
            tr("Surface temperature of this material. 0 uses the scene ambient; "
               "roughly 293 K is room temperature and 310 K is human skin."));
    });
    irLayout->addRow(temperatureCaption, m_irTemperatureSpin);

    // A temperature field instead of one temperature. The map is normalised
    // [0, 1]; what it means in kelvin is the scale and offset below it.
    auto* temperatureMapRow = new QWidget(m_irGroup);
    auto* temperatureMapLayout = new QHBoxLayout(temperatureMapRow);
    temperatureMapLayout->setContentsMargins(0, 0, 0, 0);
    m_temperatureTextureEdit = new QLineEdit(temperatureMapRow);
    connect(m_temperatureTextureEdit, &QLineEdit::editingFinished,
            this, &MaterialEditorPanel::onTemperatureMapChanged);
    m_temperatureTextureBrowse = new QPushButton(temperatureMapRow);
    m_temperatureTextureBrowse->setMaximumWidth(32);
    connect(m_temperatureTextureBrowse, &QPushButton::clicked,
            this, &MaterialEditorPanel::onBrowseTemperatureTexture);
    temperatureMapLayout->addWidget(m_temperatureTextureEdit);
    temperatureMapLayout->addWidget(m_temperatureTextureBrowse);
    auto* temperatureMapCaption = new QLabel(m_irGroup);
    bindText([this, temperatureMapCaption] {
        temperatureMapCaption->setText(tr("Temperature map:"));
        m_temperatureTextureBrowse->setText(tr("..."));
        m_temperatureTextureEdit->setToolTip(
            tr("Image whose red channel carries the temperature field, normalised "
               "to [0, 1] and mapped to kelvin by the scale and offset below. "
               "Leave empty to use the single object temperature."));
    });
    irLayout->addRow(temperatureMapCaption, temperatureMapRow);

    m_temperatureScaleSpin = new QDoubleSpinBox();
    m_temperatureScaleSpin->setRange(1.0, 2000.0);
    m_temperatureScaleSpin->setSingleStep(10.0);
    m_temperatureScaleSpin->setDecimals(1);
    connect(m_temperatureScaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MaterialEditorPanel::onTemperatureMapChanged);
    auto* temperatureScaleCaption = new QLabel(m_irGroup);
    bindText([this, temperatureScaleCaption] {
        temperatureScaleCaption->setText(tr("Map range:"));
        m_temperatureScaleSpin->setSuffix(tr(" K"));
        m_temperatureScaleSpin->setToolTip(
            tr("Kelvin spanned by the full [0, 1] of the map. The map is stored as "
               "8 bits, so this divided by 255 is the smallest temperature "
               "difference it can hold."));
    });
    irLayout->addRow(temperatureScaleCaption, m_temperatureScaleSpin);

    m_temperatureOffsetSpin = new QDoubleSpinBox();
    m_temperatureOffsetSpin->setRange(0.0, 2000.0);
    m_temperatureOffsetSpin->setSingleStep(10.0);
    m_temperatureOffsetSpin->setDecimals(1);
    connect(m_temperatureOffsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MaterialEditorPanel::onTemperatureMapChanged);
    auto* temperatureOffsetCaption = new QLabel(m_irGroup);
    bindText([this, temperatureOffsetCaption] {
        temperatureOffsetCaption->setText(tr("Map floor:"));
        m_temperatureOffsetSpin->setSuffix(tr(" K"));
        m_temperatureOffsetSpin->setToolTip(
            tr("Temperature a map value of 0 stands for."));
    });
    irLayout->addRow(temperatureOffsetCaption, m_temperatureOffsetSpin);

    m_temperatureMapNotice = new QLabel(m_irGroup);
    bindStyle([this] { uistyle::applyHintStyle(m_temperatureMapNotice); });
    irLayout->addRow(m_temperatureMapNotice);

    // Kirchhoff's law validation label
    m_irKirchhoffLabel = new QLabel();
    bindStyle([this] { uistyle::applyHintStyle(m_irKirchhoffLabel); });
    irLayout->addRow(m_irKirchhoffLabel);

    mainLayout->addWidget(m_irGroup);

    // ------------------------------------------------------------------
    // Thermal properties
    // ------------------------------------------------------------------
    // What the surface energy balance needs, when a scene runs one. Collapsed
    // by default and empty of meaning until a conductivity is set, which is
    // what opts the material into the solve.
    m_thermalGroup = new CollapsibleGroupBox();
    bindText([this] { m_thermalGroup->setTitle(tr("Thermal Properties (Solver)")); });
    auto* thermalLayout = new QFormLayout();

    auto addThermalRow = [this, thermalLayout](const QString& caption, QDoubleSpinBox* box,
                                               const QString& tip) {
        box->setToolTip(tip);
        connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &MaterialEditorPanel::onThermalPropertyChanged);
        auto* label = new QLabel(caption);
        label->setToolTip(tip);
        thermalLayout->addRow(label, box);
    };

    m_thermalConductivity = new QDoubleSpinBox();
    m_thermalConductivity->setRange(0.0, 500.0);
    m_thermalConductivity->setDecimals(3);
    m_thermalConductivity->setSingleStep(0.1);
    m_thermalConductivity->setSuffix(tr(" W/mK"));
    m_thermalConductivity->setSpecialValueText(tr("Not solved"));

    m_thermalDensity = new QDoubleSpinBox();
    m_thermalDensity->setRange(1.0, 25000.0);
    m_thermalDensity->setDecimals(0);
    m_thermalDensity->setSingleStep(100.0);
    m_thermalDensity->setSuffix(tr(" kg/m3"));
    m_thermalDensity->setValue(2000.0);

    m_thermalSpecificHeat = new QDoubleSpinBox();
    m_thermalSpecificHeat->setRange(1.0, 10000.0);
    m_thermalSpecificHeat->setDecimals(0);
    m_thermalSpecificHeat->setSingleStep(50.0);
    m_thermalSpecificHeat->setSuffix(tr(" J/kgK"));
    m_thermalSpecificHeat->setValue(900.0);

    m_thermalThickness = new QDoubleSpinBox();
    m_thermalThickness->setRange(0.001, 10.0);
    m_thermalThickness->setDecimals(3);
    m_thermalThickness->setSingleStep(0.01);
    m_thermalThickness->setSuffix(tr(" m"));
    m_thermalThickness->setValue(0.2);

    m_thermalConvection = new QDoubleSpinBox();
    m_thermalConvection->setRange(0.0, 200.0);
    m_thermalConvection->setDecimals(1);
    m_thermalConvection->setSingleStep(1.0);
    m_thermalConvection->setSuffix(tr(" W/m2K"));
    m_thermalConvection->setValue(5.0);

    m_thermalAbsorptivity = new QDoubleSpinBox();
    m_thermalAbsorptivity->setRange(0.0, 1.0);
    m_thermalAbsorptivity->setDecimals(3);
    m_thermalAbsorptivity->setSingleStep(0.05);
    m_thermalAbsorptivity->setValue(0.7);

    m_thermalWetness = new QDoubleSpinBox();
    m_thermalWetness->setRange(0.0, 1.0);
    m_thermalWetness->setDecimals(3);
    m_thermalWetness->setSingleStep(0.05);
    m_thermalWetness->setValue(0.0);

    addThermalRow(tr("Conductivity:"), m_thermalConductivity,
                  tr("How fast heat moves through the material. Zero leaves this surface "
                     "out of the solve, keeping whatever temperature it was given."));
    addThermalRow(tr("Density:"), m_thermalDensity,
                  tr("With the specific heat and the thickness, this is the thermal mass -- "
                     "how much heat the surface has to gain to warm by a degree."));
    addThermalRow(tr("Specific heat:"), m_thermalSpecificHeat,
                  tr("Heat one kilogram needs to warm by one kelvin."));
    addThermalRow(tr("Thickness:"), m_thermalThickness,
                  tr("How deep the slab is. A thin sheet follows the air within minutes; a "
                     "masonry wall takes hours, and is still warm after sunset."));
    addThermalRow(tr("Convection:"), m_thermalConvection,
                  tr("Exchange with the air. About 5 in still air, 25 in a brisk wind."));
    addThermalRow(tr("Solar absorptivity:"), m_thermalAbsorptivity,
                  tr("Fraction of sunlight absorbed. Not the infrared emissivity: fresh snow "
                     "absorbs almost no sunlight and radiates nearly as a blackbody."));
    addThermalRow(tr("Wetness factor:"), m_thermalWetness,
                  tr("How much of the surface evaporates: 0 for dry, 1 for open water. "
                     "Evaporation is why a lawn is ten degrees cooler than the pavement "
                     "beside it under the same sun."));

    // Before the back-face rows, because it decides whether any of them mean
    // anything: a shell has two exposed faces and no interior.
    m_thermalShell = new QCheckBox();
    m_thermalShell->setText(tr("Exposed on both sides"));
    m_thermalShell->setToolTip(
        tr("A panel, a sign, a tent: one thin slab with sky on both faces, "
           "modelled as two sheets of triangles. Without this the two sheets are "
           "solved as separate slabs each insulated against a wall that is not "
           "there, so a panel in the sun comes out as hot as if it were bolted to "
           "masonry. The rows below then have no interior to describe."));
    connect(m_thermalShell, &QCheckBox::toggled, this,
            &MaterialEditorPanel::onThermalPropertyChanged);
    thermalLayout->addRow(QString(), m_thermalShell);

    m_thermalInteriorBc = new QComboBox();
    m_thermalInteriorBc->addItem(tr("Adiabatic (nothing behind)"), QStringLiteral("adiabatic"));
    m_thermalInteriorBc->addItem(tr("Held at a temperature"), QStringLiteral("fixed"));
    m_thermalInteriorBc->addItem(tr("Convects to what is behind"), QStringLiteral("ambient"));
    connect(m_thermalInteriorBc, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MaterialEditorPanel::onThermalPropertyChanged);
    thermalLayout->addRow(tr("Back face:"), m_thermalInteriorBc);

    m_thermalInteriorTemp = new QDoubleSpinBox();
    m_thermalInteriorTemp->setRange(1.0, 2000.0);
    m_thermalInteriorTemp->setDecimals(1);
    m_thermalInteriorTemp->setSingleStep(1.0);
    m_thermalInteriorTemp->setSuffix(tr(" K"));
    m_thermalInteriorTemp->setValue(293.15);
    addThermalRow(tr("Behind it:"), m_thermalInteriorTemp,
                  tr("Temperature of whatever is behind the surface -- a room, usually. "
                     "Read when the back face is held at it or convects to it."));

    m_thermalInteriorConvection = new QDoubleSpinBox();
    m_thermalInteriorConvection->setRange(0.0, 500.0);
    m_thermalInteriorConvection->setDecimals(2);
    m_thermalInteriorConvection->setSingleStep(0.5);
    m_thermalInteriorConvection->setValue(3.0);
    addThermalRow(tr("Back-face convection:"), m_thermalInteriorConvection,
                  tr("Exchange between the back face and what is behind it. Lower than "
                     "the front's: still air in a closed bay rather than wind. Read only "
                     "when the back face convects."));

    m_thermalInternalHeat = new QDoubleSpinBox();
    m_thermalInternalHeat->setRange(-100000.0, 100000.0);
    m_thermalInternalHeat->setDecimals(1);
    m_thermalInternalHeat->setSingleStep(10.0);
    m_thermalInternalHeat->setValue(0.0);
    addThermalRow(tr("Internal heat:"), m_thermalInternalHeat,
                  tr("A flux entering from behind: an engine, a battery, a compartment. "
                     "The only way a shaded surface can be the warmest thing in an "
                     "infrared scene. Absorbed without trace by a back face that is "
                     "held at a temperature."));

    m_thermalNotice = new QLabel();
    bindStyle([this] { uistyle::applyHintStyle(m_thermalNotice); });
    thermalLayout->addRow(m_thermalNotice);

    m_thermalGroup->setContentLayout(thermalLayout);
    mainLayout->addWidget(m_thermalGroup);

    // ------------------------------------------------------------------
    // Transmission and volume
    // ------------------------------------------------------------------
    // These fields have always reached the GPU and the shaders have always
    // consumed them; until the core learned the matching TOML keys, only a
    // glTF KHR extension could set them, so a piece of glass could be imported
    // but not authored or edited. Collapsed by default because most materials
    // are opaque and this is five rows of nothing for them.
    m_transmissionGroup = new CollapsibleGroupBox(this);
    auto* transmissionLayout = new QFormLayout();

    m_transmissionSpin = new QDoubleSpinBox();
    m_transmissionSpin->setRange(0.0, 1.0);
    m_transmissionSpin->setSingleStep(0.05);
    m_transmissionSpin->setDecimals(3);
    connect(m_transmissionSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MaterialEditorPanel::onTransmissionChanged);
    auto* transmissionCaption = new QLabel();
    transmissionLayout->addRow(transmissionCaption, m_transmissionSpin);

    m_iorSpin = new QDoubleSpinBox();
    m_iorSpin->setRange(1.0, 4.0);
    m_iorSpin->setSingleStep(0.01);
    m_iorSpin->setDecimals(3);
    m_iorSpin->setValue(1.5);
    connect(m_iorSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MaterialEditorPanel::onTransmissionChanged);
    auto* iorCaption = new QLabel();
    transmissionLayout->addRow(iorCaption, m_iorSpin);

    m_dispersionSpin = new QDoubleSpinBox();
    m_dispersionSpin->setRange(0.0, 1.0);
    m_dispersionSpin->setSingleStep(0.005);
    m_dispersionSpin->setDecimals(4);
    connect(m_dispersionSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MaterialEditorPanel::onTransmissionChanged);
    auto* dispersionCaption = new QLabel();
    transmissionLayout->addRow(dispersionCaption, m_dispersionSpin);

    m_attenuationDistanceSpin = new QDoubleSpinBox();
    m_attenuationDistanceSpin->setRange(0.0, 1000.0);
    m_attenuationDistanceSpin->setSingleStep(0.1);
    m_attenuationDistanceSpin->setDecimals(3);
    connect(m_attenuationDistanceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MaterialEditorPanel::onTransmissionChanged);
    auto* attenuationCaption = new QLabel();
    transmissionLayout->addRow(attenuationCaption, m_attenuationDistanceSpin);

    m_attenuationColorBtn = new QPushButton();
    m_attenuationColorBtn->setFixedSize(80, 30);
    connect(m_attenuationColorBtn, &QPushButton::clicked,
            this, &MaterialEditorPanel::onAttenuationColorClicked);
    auto* attenuationColourCaption = new QLabel();
    transmissionLayout->addRow(attenuationColourCaption, m_attenuationColorBtn);

    m_transmissionGroup->setContentLayout(transmissionLayout);
    mainLayout->addWidget(m_transmissionGroup);

    bindText([this, transmissionCaption, iorCaption, dispersionCaption,
              attenuationCaption, attenuationColourCaption] {
        m_transmissionGroup->setTitle(tr("Transmission and Volume"));
        transmissionCaption->setText(tr("Transmission:"));
        m_transmissionSpin->setToolTip(
            tr("How much light passes through rather than reflecting. 0 is opaque."));
        iorCaption->setText(tr("IOR:"));
        m_iorSpin->setToolTip(
            tr("Index of refraction: 1.0 air, 1.33 water, 1.5 glass, 2.4 diamond."));
        dispersionCaption->setText(tr("Dispersion:"));
        m_dispersionSpin->setToolTip(
            tr("Reciprocal Abbe number — how much the index varies with wavelength. "
               "0 is no dispersion; this is what splits white light in a prism, and "
               "it is only visible in the spectral modes."));
        attenuationCaption->setText(tr("Attenuation distance:"));
        m_attenuationDistanceSpin->setToolTip(
            tr("Distance inside the medium at which light reaches the attenuation "
               "colour (Beer-Lambert). 0 means no absorption at all."));
        attenuationColourCaption->setText(tr("Attenuation colour:"));
    });

    // ------------------------------------------------------------------
    // Sheen
    // ------------------------------------------------------------------
    // The microfibre lobe: velvet, felt, brushed cloth. Its own group rather
    // than two more rows under Metallic-Roughness, because it is not a
    // variation on the base lobe -- it peaks where the surface turns away from
    // the camera, which no roughness setting reproduces. Collapsed by default
    // for the same reason Transmission is: most materials are not cloth.
    m_sheenGroup = new CollapsibleGroupBox(this);
    auto* sheenLayout = new QFormLayout();

    m_sheenColorBtn = new QPushButton();
    m_sheenColorBtn->setFixedSize(80, 30);
    connect(m_sheenColorBtn, &QPushButton::clicked,
            this, &MaterialEditorPanel::onSheenColorClicked);
    auto* sheenColourCaption = new QLabel();
    sheenLayout->addRow(sheenColourCaption, m_sheenColorBtn);

    m_sheenRoughnessSpin = new QDoubleSpinBox();
    m_sheenRoughnessSpin->setRange(0.0, 1.0);
    m_sheenRoughnessSpin->setSingleStep(0.05);
    m_sheenRoughnessSpin->setDecimals(3);
    connect(m_sheenRoughnessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MaterialEditorPanel::onSheenChanged);
    auto* sheenRoughnessCaption = new QLabel();
    sheenLayout->addRow(sheenRoughnessCaption, m_sheenRoughnessSpin);

    m_sheenGroup->setContentLayout(sheenLayout);
    mainLayout->addWidget(m_sheenGroup);

    bindText([this, sheenColourCaption, sheenRoughnessCaption] {
        m_sheenGroup->setTitle(tr("Sheen"));
        sheenColourCaption->setText(tr("Sheen colour:"));
        m_sheenColorBtn->setToolTip(
            tr("Colour and strength of the microfibre lobe — velvet, felt, brushed "
               "cloth. Black is no sheen, which is the default. The lobe is brightest "
               "where the surface turns away from the camera, so it reads as a rim "
               "rather than a highlight."));
        sheenRoughnessCaption->setText(tr("Sheen roughness:"));
        m_sheenRoughnessSpin->setToolTip(
            tr("How spread out the fibre scattering is. Low values give a tight bright "
               "rim, high values a broad sheen over the whole surface. Independent of "
               "the roughness above, which describes the surface under the fibres."));
    });

    // ------------------------------------------------------------------
    // Surface extensions
    // ------------------------------------------------------------------
    // Specular, anisotropy, clearcoat and diffuse transmission share one group.
    // A material carrying any of them usually carries only one, and four empty
    // boxes read worse than one. Collapsed unless the material already uses
    // something here.
    m_surfaceGroup = new CollapsibleGroupBox(this);
    auto* surfaceLayout = new QFormLayout();

    const auto addSpin = [&](QDoubleSpinBox*& spin, double lo, double hi, double step,
                             int decimals) {
        spin = new QDoubleSpinBox();
        spin->setRange(lo, hi);
        spin->setSingleStep(step);
        spin->setDecimals(decimals);
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &MaterialEditorPanel::onSurfaceExtensionChanged);
        auto* caption = new QLabel();
        surfaceLayout->addRow(caption, spin);
        return caption;
    };

    auto* specularCaption = addSpin(m_specularSpin, 0.0, 1.0, 0.05, 3);

    m_specularColorBtn = new QPushButton();
    m_specularColorBtn->setFixedSize(80, 30);
    connect(m_specularColorBtn, &QPushButton::clicked,
            this, &MaterialEditorPanel::onSpecularColorClicked);
    auto* specularColourCaption = new QLabel();
    surfaceLayout->addRow(specularColourCaption, m_specularColorBtn);

    auto* anisoCaption = addSpin(m_anisotropyStrengthSpin, 0.0, 1.0, 0.05, 3);
    auto* anisoRotCaption = addSpin(m_anisotropyRotationSpin, -3.15, 3.15, 0.05, 4);
    auto* coatCaption = addSpin(m_clearcoatSpin, 0.0, 1.0, 0.05, 3);
    auto* coatRoughCaption = addSpin(m_clearcoatRoughnessSpin, 0.0, 1.0, 0.05, 3);
    auto* dtCaption = addSpin(m_diffuseTransmissionSpin, 0.0, 1.0, 0.05, 3);

    m_alphaModeCombo = new QComboBox();
    m_alphaModeCombo->addItem(QString(), 0);
    m_alphaModeCombo->addItem(QString(), 1);
    m_alphaModeCombo->addItem(QString(), 2);
    connect(m_alphaModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MaterialEditorPanel::onAlphaModeChanged);
    auto* alphaModeCaption = new QLabel();
    surfaceLayout->addRow(alphaModeCaption, m_alphaModeCombo);

    auto* alphaCutoffCaption = addSpin(m_alphaCutoffSpin, 0.0, 1.0, 0.05, 3);

    m_diffuseTransmissionColorBtn = new QPushButton();
    m_diffuseTransmissionColorBtn->setFixedSize(80, 30);
    connect(m_diffuseTransmissionColorBtn, &QPushButton::clicked,
            this, &MaterialEditorPanel::onDiffuseTransmissionColorClicked);
    auto* dtColourCaption = new QLabel();
    surfaceLayout->addRow(dtColourCaption, m_diffuseTransmissionColorBtn);

    m_surfaceGroup->setContentLayout(surfaceLayout);
    mainLayout->addWidget(m_surfaceGroup);

    bindText([this, specularCaption, specularColourCaption, anisoCaption, anisoRotCaption,
              coatCaption, coatRoughCaption, dtCaption, dtColourCaption,
              alphaModeCaption, alphaCutoffCaption] {
        m_surfaceGroup->setTitle(tr("Surface extensions"));

        specularCaption->setText(tr("Specular:"));
        m_specularSpin->setToolTip(
            tr("How much dielectric reflection this surface has, from none to the "
               "full amount its refractive index implies. The default is 1, not 0 — "
               "lowering it removes the highlight a plastic or a painted surface "
               "would otherwise have. Metals are unaffected."));
        specularColourCaption->setText(tr("Specular colour:"));
        m_specularColorBtn->setToolTip(
            tr("Tints the dielectric reflection. White leaves it alone. Black removes "
               "the highlight seen head-on while leaving the rim at grazing angles, "
               "which is how some fabrics are authored."));

        anisoCaption->setText(tr("Anisotropy:"));
        m_anisotropyStrengthSpin->setToolTip(
            tr("Stretches the highlight along the surface's tangent — brushed metal, "
               "hair, vinyl. Zero is a round highlight. This only ever roughens the "
               "one direction; it never sharpens the other.\n\nA mesh with no tangent "
               "data gets an arbitrary direction, and the log says so when it "
               "happens."));
        anisoRotCaption->setText(tr("Anisotropy rotation:"));
        m_anisotropyRotationSpin->setToolTip(
            tr("Turns the stretch direction, in radians, counter-clockwise from the "
               "tangent. Has no visible effect while the strength above is zero."));

        coatCaption->setText(tr("Clearcoat:"));
        m_clearcoatSpin->setToolTip(
            tr("A thin lacquer over everything else — car paint, varnished wood. It "
               "adds its own reflection and dims what is underneath, emission "
               "included, by exactly what it reflects away.\n\nIn the thermal bands "
               "this does nothing on its own: a lacquer's visible 4% reflectance is "
               "fiction at 10 microns, so those bands act on a measured curve or not "
               "at all."));
        coatRoughCaption->setText(tr("Clearcoat roughness:"));
        m_clearcoatRoughnessSpin->setToolTip(
            tr("How polished the coat is, independent of the surface beneath it. "
               "Zero is a mirror finish."));

        dtCaption->setText(tr("Diffuse transmission:"));
        m_diffuseTransmissionSpin->setToolTip(
            tr("Light scattered through the surface and out the other side — leaves, "
               "paper, thin porcelain. It takes energy from the diffuse reflection "
               "rather than adding to it, so at 1 the surface reflects no diffuse at "
               "all and is only lit from behind."));
        dtColourCaption->setText(tr("Transmission colour:"));
        m_diffuseTransmissionColorBtn->setToolTip(
            tr("Tints what passes through. White passes everything the factor allows."));

        alphaModeCaption->setText(tr("Alpha mode:"));
        m_alphaModeCombo->setItemText(0, tr("Opaque"));
        m_alphaModeCombo->setItemText(1, tr("Mask (cut-out)"));
        m_alphaModeCombo->setItemText(2, tr("Blend"));
        m_alphaModeCombo->setToolTip(
            tr("How the base colour's alpha is read.\n\n"
               "Opaque ignores it. Mask makes a hole wherever alpha falls below the "
               "cutoff — a crisp edge, which is what foliage and chain-link want. "
               "Blend treats alpha as partial coverage and resolves it across "
               "samples, so it looks grainy until the render settles.\n\n"
               "A hole is a hole in every band: in the infrared you see whatever is "
               "behind it, at that thing's own temperature.\n\n"
               "This has no effect on a material that also has transmission — "
               "refraction and coverage are different things, and applying both "
               "would remove the surface twice."));
        alphaCutoffCaption->setText(tr("Alpha cutoff:"));
        m_alphaCutoffSpin->setToolTip(
            tr("Where Mask puts the boundary. Alpha at or above this is surface, "
               "below it is hole. Ignored by the other two modes."));
    });

    mainLayout->addStretch();

    updateKirchhoffLabel();

    // Disable by default. QWidget::setEnabled -- "this panel is not
    // interactive" -- not a material property.
    setEnabled(false);
}

void MaterialEditorPanel::retranslateUi() {
    PanelBase::retranslateUi();
    updateKirchhoffLabel();
    updateSpectralCurveNotice();
}

void MaterialEditorPanel::restyleUi() {
    PanelBase::restyleUi();
    // The Kirchhoff label is one of two colours depending on whether energy
    // conservation currently holds, so the bound setter above can only restore
    // the neutral one. Re-deciding is the same call the value change makes.
    updateKirchhoffLabel();
}

void MaterialEditorPanel::setMaterial(int index, const quantiloom::Material* material) {
    m_currentIndex = index;
    m_currentMaterial = material;
    m_overwriteConfirmed = false;

    if (!material) {
        clear();
        return;
    }

    setEnabled(true);

    const QString name = material->name.empty()
        ? tr("Material %1").arg(index)
        : QString::fromStdString(material->name);
    m_materialName->setText(name);

    m_baseColor = material->baseColorFactor;
    updateColorButton(m_baseColorBtn, glm::vec3(m_baseColor));

    m_metallic = material->metallicFactor;
    {
        const QSignalBlocker slider(m_metallicSlider);
        const QSignalBlocker spin(m_metallicSpin);
        m_metallicSlider->setValue(static_cast<int>(m_metallic * 100));
        m_metallicSpin->setValue(m_metallic);
    }

    m_roughness = material->roughnessFactor;
    {
        const QSignalBlocker slider(m_roughnessSlider);
        const QSignalBlocker spin(m_roughnessSpin);
        m_roughnessSlider->setValue(static_cast<int>(m_roughness * 100));
        m_roughnessSpin->setValue(m_roughness);
    }

    // Transmission and volume, read back so the panel shows what the glTF or
    // the config actually loaded rather than this editor's defaults.
    m_transmission = material->transmission;
    m_ior = material->ior;
    m_dispersion = material->dispersion;
    m_attenuationDistance = material->attenuationDistance;
    m_attenuationColor = material->attenuationColor;
    {
        const QSignalBlocker b1(m_transmissionSpin);
        const QSignalBlocker b2(m_iorSpin);
        const QSignalBlocker b3(m_dispersionSpin);
        const QSignalBlocker b4(m_attenuationDistanceSpin);
        m_transmissionSpin->setValue(m_transmission);
        m_iorSpin->setValue(m_ior);
        m_dispersionSpin->setValue(m_dispersion);
        m_attenuationDistanceSpin->setValue(m_attenuationDistance);
    }
    updateColorButton(m_attenuationColorBtn, m_attenuationColor);
    // Opened when the material is actually transmissive: a glass that arrives
    // from a glTF should not need to be hunted for behind a collapsed header.
    if (m_transmission > 0.0f) {
        m_transmissionGroup->setCollapsed(false);
    }

    // Sheen, read back for the same reason: a velvet imported from a glTF
    // should show the factors it arrived with, not this editor's defaults.
    m_sheenColor = material->sheenColorFactor;
    m_sheenRoughness = material->sheenRoughnessFactor;
    {
        const QSignalBlocker b(m_sheenRoughnessSpin);
        m_sheenRoughnessSpin->setValue(m_sheenRoughness);
    }
    updateColorButton(m_sheenColorBtn, m_sheenColor);
    if (material->HasSheen()) {
        m_sheenGroup->setCollapsed(false);
    }

    // The four surface extensions, read back the same way. Specular is the one
    // whose neutral value is 1 rather than 0, so a material that never named it
    // still shows a full dielectric response here -- which is what it has.
    m_alphaMode = static_cast<int>(material->alphaMode);
    m_alphaCutoff = material->alphaCutoff;
    m_specular = material->specularFactor;
    m_specularColor = material->specularColorFactor;
    m_anisotropyStrength = material->anisotropyStrength;
    m_anisotropyRotation = material->anisotropyRotation;
    m_clearcoat = material->clearcoatFactor;
    m_clearcoatRoughness = material->clearcoatRoughnessFactor;
    m_diffuseTransmission = material->diffuseTransmissionFactor;
    m_diffuseTransmissionColor = material->diffuseTransmissionColorFactor;
    {
        const QSignalBlocker a(m_specularSpin);
        const QSignalBlocker b(m_anisotropyStrengthSpin);
        const QSignalBlocker c(m_anisotropyRotationSpin);
        const QSignalBlocker d(m_clearcoatSpin);
        const QSignalBlocker e(m_clearcoatRoughnessSpin);
        const QSignalBlocker f(m_diffuseTransmissionSpin);
        const QSignalBlocker g(m_alphaModeCombo);
        const QSignalBlocker h(m_alphaCutoffSpin);
        m_alphaModeCombo->setCurrentIndex(m_alphaMode);
        m_alphaCutoffSpin->setValue(m_alphaCutoff);
        m_specularSpin->setValue(m_specular);
        m_anisotropyStrengthSpin->setValue(m_anisotropyStrength);
        m_anisotropyRotationSpin->setValue(m_anisotropyRotation);
        m_clearcoatSpin->setValue(m_clearcoat);
        m_clearcoatRoughnessSpin->setValue(m_clearcoatRoughness);
        m_diffuseTransmissionSpin->setValue(m_diffuseTransmission);
    }
    updateColorButton(m_specularColorBtn, m_specularColor);
    updateColorButton(m_diffuseTransmissionColorBtn, m_diffuseTransmissionColor);
    if (material->HasSpecular() || material->HasAnisotropy() || material->HasClearcoat() ||
        material->HasDiffuseTransmission() ||
        material->alphaMode != quantiloom::Material::AlphaMode::Opaque) {
        m_surfaceGroup->setCollapsed(false);
    }

    m_emissive = material->emissiveFactor;
    {
        const QSignalBlocker r(m_emissiveR);
        const QSignalBlocker g(m_emissiveG);
        const QSignalBlocker b(m_emissiveB);
        m_emissiveR->setValue(m_emissive.r);
        m_emissiveG->setValue(m_emissive.g);
        m_emissiveB->setValue(m_emissive.b);
    }

    m_emissiveCurveSource = QString::fromStdString(material->emissiveCurveSource);
    {
        const QSignalBlocker c(m_emissiveCurve);
        int index = m_emissiveCurve->findData(m_emissiveCurveSource);
        if (index < 0 && !m_emissiveCurveSource.isEmpty()) {
            // A path, or a token this build does not know. Offered back rather
            // than silently reset to "None": dropping it here would unbind a
            // lamp the config deliberately bound, and the user would find out
            // by saving.
            m_emissiveCurve->addItem(m_emissiveCurveSource, m_emissiveCurveSource);
            index = m_emissiveCurve->count() - 1;
        }
        m_emissiveCurve->setCurrentIndex(index < 0 ? 0 : index);
    }
    updateEmissiveCurveNote();

    // IR properties: the scalar view of what may be a full curve.
    m_irEmissivity = material->irEmissivityCurve.empty()
        ? 0.0f : material->irEmissivityCurve[0].second;
    m_irTransmittance = material->irTransmittanceCurve.empty()
        ? 0.0f : material->irTransmittanceCurve[0].second;
    m_irTemperature_K = material->irTemperature_K;

    m_hasGeneratedCurves =
        material->irEmissivityCurve.size() > kSimpleCurvePoints ||
        material->irTransmittanceCurve.size() > kSimpleCurvePoints ||
        material->irReflectanceCurve.size() > kSimpleCurvePoints;

    m_temperatureTexture = QString::fromStdString(material->temperatureTexturePath);
    m_temperatureScale = material->temperatureScale;
    m_temperatureOffset = material->temperatureOffset;

    {
        const QSignalBlocker e(m_irEmissivitySpin);
        const QSignalBlocker t(m_irTransmittanceSpin);
        const QSignalBlocker k(m_irTemperatureSpin);
        const QSignalBlocker p(m_temperatureTextureEdit);
        const QSignalBlocker s(m_temperatureScaleSpin);
        const QSignalBlocker o(m_temperatureOffsetSpin);
        m_irEmissivitySpin->setValue(static_cast<double>(m_irEmissivity));
        m_irTransmittanceSpin->setValue(static_cast<double>(m_irTransmittance));
        m_irTemperatureSpin->setValue(static_cast<double>(m_irTemperature_K));
        m_temperatureTextureEdit->setText(m_temperatureTexture);
        m_temperatureScaleSpin->setValue(static_cast<double>(m_temperatureScale));
        m_temperatureOffsetSpin->setValue(static_cast<double>(m_temperatureOffset));
    }

    updateSpectralCurveNotice();
    updateTemperatureMapNotice();
    updateKirchhoffLabel();
}

void MaterialEditorPanel::clear() {
    m_currentIndex = -1;
    m_currentMaterial = nullptr;
    m_hasGeneratedCurves = false;
    m_overwriteConfirmed = false;
    m_materialName->setText(tr("No material selected"));
    m_spectralCurveNotice->setVisible(false);
    setEnabled(false);
}

void MaterialEditorPanel::updateSpectralCurveNotice() {
    if (!m_spectralCurveNotice) {
        return;
    }
    m_spectralCurveNotice->setVisible(m_hasGeneratedCurves);
    if (m_hasGeneratedCurves) {
        m_spectralCurveNotice->setText(tr(
            "This material carries full spectral IR curves from the material "
            "generator. The three values below are a two-point summary of them; "
            "editing one replaces the curves."));
    }
}

bool MaterialEditorPanel::confirmSpectralOverwrite() {
    if (!m_hasGeneratedCurves || m_overwriteConfirmed) {
        return true;
    }

    const auto reply = QMessageBox::question(
        this,
        tr("Replace spectral curves?"),
        tr("This material's infrared response is stored as full spectral curves. "
           "Editing this value replaces them with two constant sample points.\n\n"
           "Replace the curves?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_overwriteConfirmed = true;
        return true;
    }
    return false;
}

void MaterialEditorPanel::updateColorButton(QPushButton* btn, const glm::vec3& color) {
    const int r = static_cast<int>(color.r * 255);
    const int g = static_cast<int>(color.g * 255);
    const int b = static_cast<int>(color.b * 255);
    btn->setStyleSheet(QStringLiteral("background-color: rgb(%1, %2, %3);").arg(r).arg(g).arg(b));
}

void MaterialEditorPanel::onBaseColorClicked() {
    const QColor initial = QColor::fromRgbF(m_baseColor.r, m_baseColor.g, m_baseColor.b);
    const QColor color = QColorDialog::getColor(initial, this, tr("Select Base Color"));

    if (color.isValid()) {
        m_baseColor = glm::vec4(color.redF(), color.greenF(), color.blueF(), m_baseColor.a);
        updateColorButton(m_baseColorBtn, glm::vec3(m_baseColor));
        applyChanges();
    }
}

void MaterialEditorPanel::onMetallicChanged(int value) {
    m_metallic = value / 100.0f;
    {
        const QSignalBlocker blocker(m_metallicSpin);
        m_metallicSpin->setValue(m_metallic);
    }
    applyChanges();
}

void MaterialEditorPanel::onMetallicSpinChanged(double value) {
    m_metallic = static_cast<float>(value);
    {
        // The slider is the coarser of the two, so it follows without
        // rounding the typed value back.
        const QSignalBlocker blocker(m_metallicSlider);
        m_metallicSlider->setValue(static_cast<int>(std::lround(value * 100.0)));
    }
    applyChanges();
}

void MaterialEditorPanel::onRoughnessChanged(int value) {
    m_roughness = value / 100.0f;
    {
        const QSignalBlocker blocker(m_roughnessSpin);
        m_roughnessSpin->setValue(m_roughness);
    }
    applyChanges();
}

void MaterialEditorPanel::onRoughnessSpinChanged(double value) {
    m_roughness = static_cast<float>(value);
    {
        const QSignalBlocker blocker(m_roughnessSlider);
        m_roughnessSlider->setValue(static_cast<int>(std::lround(value * 100.0)));
    }
    applyChanges();
}

void MaterialEditorPanel::onEmissiveChanged() {
    m_emissive = glm::vec3(
        static_cast<float>(m_emissiveR->value()),
        static_cast<float>(m_emissiveG->value()),
        static_cast<float>(m_emissiveB->value())
    );
    applyChanges();
}

void MaterialEditorPanel::onEmissiveCurveChanged() {
    m_emissiveCurveSource = m_emissiveCurve->currentData().toString();
    updateEmissiveCurveNote();
    applyChanges();
}

void MaterialEditorPanel::updateEmissiveCurveNote() {
    if (!m_emissiveCurveNote || !m_emissiveCurve || !m_emissiveScale) {
        return;
    }

    const QString token = m_emissiveCurve->currentData().toString();
    const bool bound = !token.isEmpty();
    m_emissiveScale->setEnabled(bound);

    if (!bound) {
        m_emissiveCurveNote->setText(
            tr("No spectrum: the RGB above is expanded as sigmoid(rgb) × D65, which is "
               "a convention rather than a measurement and is defined only over "
               "380–780 nm. The SWIR, NIR, MWIR and LWIR bands read no emissive RGB, "
               "so this light does not exist for them."));
        return;
    }

    // The span is the whole point of showing anything here: emission is zero
    // outside the measured range rather than held flat at the endpoint, so a
    // lamp tabulated to 780 nm is genuinely dark in an infrared render and the
    // user should learn that here rather than from a black image.
    for (const auto& info : quantiloom::SpectralIO::BuiltinEmissionSpectra()) {
        if (QString::fromStdString(info.token) != token) {
            continue;
        }
        const auto lo = static_cast<int>(info.lambdaMinNm);
        const auto hi = static_cast<int>(info.lambdaMaxNm);
        QString text = tr("Measured over %1–%2 nm. Emission is zero outside that span, "
                          "never held flat, so a band beyond it renders this light dark "
                          "rather than inventing output for it.").arg(lo).arg(hi);
        if (hi <= 800) {
            text += QLatin1Char(' ');
            text += tr("This one stops at the visible: use blackbody_<T>k for an "
                       "infrared band, which is a formula and spans every band.");
        }
        m_emissiveCurveNote->setText(text);
        return;
    }

    m_emissiveCurveNote->setText(
        tr("Bound to '%1'. Emission is zero outside the table's own span, never held "
           "flat.").arg(token));
}

void MaterialEditorPanel::onIRPropertyChanged() {
    if (!confirmSpectralOverwrite()) {
        // Put the widgets back where they were; nothing is emitted.
        const QSignalBlocker e(m_irEmissivitySpin);
        const QSignalBlocker t(m_irTransmittanceSpin);
        const QSignalBlocker k(m_irTemperatureSpin);
        m_irEmissivitySpin->setValue(static_cast<double>(m_irEmissivity));
        m_irTransmittanceSpin->setValue(static_cast<double>(m_irTransmittance));
        m_irTemperatureSpin->setValue(static_cast<double>(m_irTemperature_K));
        return;
    }

    m_irEmissivity = static_cast<float>(m_irEmissivitySpin->value());
    m_irTransmittance = static_cast<float>(m_irTransmittanceSpin->value());
    m_irTemperature_K = static_cast<float>(m_irTemperatureSpin->value());

    // Once the constants are written the curves are gone, so the notice is no
    // longer true.
    m_hasGeneratedCurves = false;
    updateSpectralCurveNotice();

    updateKirchhoffLabel();
    applyChanges();
}

void MaterialEditorPanel::onTemperatureMapChanged() {
    m_temperatureTexture = m_temperatureTextureEdit->text().trimmed();
    m_temperatureScale = static_cast<float>(m_temperatureScaleSpin->value());
    m_temperatureOffset = static_cast<float>(m_temperatureOffsetSpin->value());

    updateTemperatureMapNotice();
    applyChanges();
}

void MaterialEditorPanel::onBrowseTemperatureTexture() {
    const QString chosen = QFileDialog::getOpenFileName(
        this, tr("Temperature Map"), m_temperatureTextureEdit->text(),
        tr("Images (*.png *.exr *.tif *.tiff *.hdr *.jpg);;All files (*)"));
    if (chosen.isEmpty()) {
        return;
    }
    m_temperatureTextureEdit->setText(chosen);
    onTemperatureMapChanged();
}

void MaterialEditorPanel::updateTemperatureMapNotice() {
    if (!m_temperatureMapNotice) {
        return;
    }
    if (m_temperatureTexture.isEmpty()) {
        m_temperatureMapNotice->setText(
            tr("No map: this surface is one temperature."));
        return;
    }
    // The step is what an author needs to choose a range by, and the reload is
    // what stops a changed path reading as a change that did nothing: the core
    // mounts temperature maps when the scene loads, and the viewport is
    // showing the one it mounted.
    m_temperatureMapNotice->setText(
        tr("Map covers %1 K to %2 K in steps of %3 K. A changed path takes "
           "effect when the scene is reloaded.")
            .arg(static_cast<double>(m_temperatureOffset), 0, 'f', 1)
            .arg(static_cast<double>(m_temperatureOffset + m_temperatureScale), 0, 'f', 1)
            .arg(static_cast<double>(m_temperatureScale) / 255.0, 0, 'f', 2));
}

void MaterialEditorPanel::setThermalProperties(const MaterialThermalProps& props) {
    m_thermal = props;

    const QSignalBlocker k(m_thermalConductivity);
    const QSignalBlocker rho(m_thermalDensity);
    const QSignalBlocker c(m_thermalSpecificHeat);
    const QSignalBlocker d(m_thermalThickness);
    const QSignalBlocker h(m_thermalConvection);
    const QSignalBlocker a(m_thermalAbsorptivity);
    const QSignalBlocker w(m_thermalWetness);
    const QSignalBlocker bc(m_thermalInteriorBc);
    const QSignalBlocker t(m_thermalInteriorTemp);
    const QSignalBlocker ic(m_thermalInteriorConvection);
    const QSignalBlocker q(m_thermalInternalHeat);
    const QSignalBlocker sh(m_thermalShell);

    m_thermalConductivity->setValue(static_cast<double>(props.conductivity));
    m_thermalDensity->setValue(static_cast<double>(props.density));
    m_thermalSpecificHeat->setValue(static_cast<double>(props.specificHeat));
    m_thermalThickness->setValue(static_cast<double>(props.thickness));
    m_thermalConvection->setValue(static_cast<double>(props.convection));
    m_thermalAbsorptivity->setValue(static_cast<double>(props.shortwaveAbsorptivity));
    m_thermalWetness->setValue(static_cast<double>(props.wetness));
    m_thermalInteriorTemp->setValue(static_cast<double>(props.interiorTemperature));
    m_thermalInteriorConvection->setValue(static_cast<double>(props.interiorConvection));
    m_thermalInternalHeat->setValue(static_cast<double>(props.internalHeat));
    m_thermalShell->setChecked(props.isShell);
    const int index = m_thermalInteriorBc->findData(props.interiorBoundary);
    m_thermalInteriorBc->setCurrentIndex(index >= 0 ? index : 0);

    // A material that is being solved is worth unfolding for.
    if (props.conductivity > 0.0f) {
        m_thermalGroup->setCollapsed(false);
    }
    updateThermalNotice();
}

void MaterialEditorPanel::onThermalPropertyChanged() {
    m_thermal.conductivity = static_cast<float>(m_thermalConductivity->value());
    m_thermal.density = static_cast<float>(m_thermalDensity->value());
    m_thermal.specificHeat = static_cast<float>(m_thermalSpecificHeat->value());
    m_thermal.thickness = static_cast<float>(m_thermalThickness->value());
    m_thermal.convection = static_cast<float>(m_thermalConvection->value());
    m_thermal.shortwaveAbsorptivity = static_cast<float>(m_thermalAbsorptivity->value());
    m_thermal.wetness = static_cast<float>(m_thermalWetness->value());
    m_thermal.interiorTemperature = static_cast<float>(m_thermalInteriorTemp->value());
    m_thermal.interiorConvection = static_cast<float>(m_thermalInteriorConvection->value());
    m_thermal.internalHeat = static_cast<float>(m_thermalInternalHeat->value());
    m_thermal.isShell = m_thermalShell->isChecked();
    m_thermal.interiorBoundary = m_thermalInteriorBc->currentData().toString();

    updateThermalNotice();

    if (m_currentIndex >= 0) {
        emit thermalPropertiesChanged(m_currentIndex, m_thermal);
    }
}

void MaterialEditorPanel::updateThermalNotice() {
    if (!m_thermalNotice) {
        return;
    }
    // Which of the three back-face rows is read, so a number that does nothing
    // looks like a number that does nothing.
    // A shell is exposed on both sides, so there is no interior for any of the
    // back-face rows to describe.
    const bool shell = m_thermalShell->isChecked();
    const QString boundary = m_thermalInteriorBc->currentData().toString();
    const bool held = !shell && boundary == QLatin1String("fixed");
    const bool convects = !shell && boundary == QLatin1String("ambient");
    m_thermalInteriorBc->setEnabled(!shell);
    m_thermalInteriorTemp->setEnabled(held || convects);
    m_thermalInteriorConvection->setEnabled(convects);
    // A pinned back node absorbs whatever is put into it, so a source there
    // changes nothing -- the same thing the SDK warns about when a config says
    // both.
    m_thermalInternalHeat->setEnabled(!held && !shell);

    if (m_thermal.conductivity <= 0.0f) {
        m_thermalNotice->setText(
            tr("Not solved: this surface keeps the temperature it is given."));
        return;
    }

    // The time constant is what the numbers above amount to, and it is the one
    // figure that says how this surface will behave: a few minutes and it
    // follows the air, a few hours and it is still warm after sunset.
    const double capacity = static_cast<double>(m_thermal.density) *
                            m_thermal.specificHeat * m_thermal.thickness;
    const double loss = static_cast<double>(m_thermal.convection) + 5.5;  // + radiative, near 300 K
    const double tauHours = loss > 0.0 ? capacity / loss / 3600.0 : 0.0;

    m_thermalNotice->setText(
        tr("Time constant about %1 h: how long this surface takes to follow a change "
           "in the air around it.")
            .arg(tauHours, 0, 'f', 1));
}

void MaterialEditorPanel::updateKirchhoffLabel() {
    if (!m_irKirchhoffLabel) {
        return;
    }

    // Validate Kirchhoff's law: emissivity + reflectance + transmittance <= 1
    // Reflectance is computed as: rho = 1 - epsilon - tau
    const float reflectance = 1.0f - m_irEmissivity - m_irTransmittance;

    if (m_irEmissivity + m_irTransmittance > 1.0f) {
        m_irKirchhoffLabel->setText(
            tr("Warning: ε + τ > 1 (violates energy conservation)"));
        QPalette pal = m_irKirchhoffLabel->palette();
        pal.setColor(QPalette::WindowText,
                     ThemeManager::instance().currentTheme().accents.errorText);
        m_irKirchhoffLabel->setPalette(pal);
    } else {
        uistyle::applyHintStyle(m_irKirchhoffLabel);
        if (m_irEmissivity > 0.0f || m_irTransmittance > 0.0f) {
            m_irKirchhoffLabel->setText(tr("Reflectance ρ = %1").arg(reflectance, 0, 'f', 3));
        } else {
            m_irKirchhoffLabel->setText(tr("Set IR properties for thermal rendering"));
        }
    }
}

void MaterialEditorPanel::onTransmissionChanged() {
    m_transmission = static_cast<float>(m_transmissionSpin->value());
    m_ior = static_cast<float>(m_iorSpin->value());
    m_dispersion = static_cast<float>(m_dispersionSpin->value());
    m_attenuationDistance = static_cast<float>(m_attenuationDistanceSpin->value());
    applyChanges();
}

void MaterialEditorPanel::onAttenuationColorClicked() {
    const QColor initial = QColor::fromRgbF(m_attenuationColor.r, m_attenuationColor.g,
                                            m_attenuationColor.b);
    const QColor chosen = QColorDialog::getColor(initial, this, tr("Attenuation Colour"));
    if (!chosen.isValid()) {
        return;
    }
    m_attenuationColor = glm::vec3(static_cast<float>(chosen.redF()),
                                   static_cast<float>(chosen.greenF()),
                                   static_cast<float>(chosen.blueF()));
    updateColorButton(m_attenuationColorBtn, m_attenuationColor);
    applyChanges();
}

void MaterialEditorPanel::onSheenChanged() {
    m_sheenRoughness = static_cast<float>(m_sheenRoughnessSpin->value());
    applyChanges();
}

void MaterialEditorPanel::onSurfaceExtensionChanged() {
    m_specular = static_cast<float>(m_specularSpin->value());
    m_anisotropyStrength = static_cast<float>(m_anisotropyStrengthSpin->value());
    m_anisotropyRotation = static_cast<float>(m_anisotropyRotationSpin->value());
    m_clearcoat = static_cast<float>(m_clearcoatSpin->value());
    m_clearcoatRoughness = static_cast<float>(m_clearcoatRoughnessSpin->value());
    m_diffuseTransmission = static_cast<float>(m_diffuseTransmissionSpin->value());
    m_alphaCutoff = static_cast<float>(m_alphaCutoffSpin->value());
    applyChanges();
}

void MaterialEditorPanel::onAlphaModeChanged() {
    m_alphaMode = m_alphaModeCombo->currentIndex();
    applyChanges();
}

void MaterialEditorPanel::onSpecularColorClicked() {
    const QColor initial =
        QColor::fromRgbF(qMin(m_specularColor.r, 1.0f), qMin(m_specularColor.g, 1.0f),
                         qMin(m_specularColor.b, 1.0f));
    const QColor chosen = QColorDialog::getColor(initial, this, tr("Specular Colour"));
    if (!chosen.isValid()) {
        return;
    }
    // The picker cannot express the above-1 values glTF allows here -- an asset
    // may author [10, 0.6, 0] to saturate one channel -- so editing the colour
    // clamps it into range. Reading one back and not touching it does not.
    m_specularColor = glm::vec3(static_cast<float>(chosen.redF()),
                                static_cast<float>(chosen.greenF()),
                                static_cast<float>(chosen.blueF()));
    updateColorButton(m_specularColorBtn, m_specularColor);
    applyChanges();
}

void MaterialEditorPanel::onDiffuseTransmissionColorClicked() {
    const QColor initial = QColor::fromRgbF(m_diffuseTransmissionColor.r,
                                            m_diffuseTransmissionColor.g,
                                            m_diffuseTransmissionColor.b);
    const QColor chosen =
        QColorDialog::getColor(initial, this, tr("Diffuse Transmission Colour"));
    if (!chosen.isValid()) {
        return;
    }
    m_diffuseTransmissionColor = glm::vec3(static_cast<float>(chosen.redF()),
                                           static_cast<float>(chosen.greenF()),
                                           static_cast<float>(chosen.blueF()));
    updateColorButton(m_diffuseTransmissionColorBtn, m_diffuseTransmissionColor);
    applyChanges();
}

void MaterialEditorPanel::onSheenColorClicked() {
    const QColor initial = QColor::fromRgbF(m_sheenColor.r, m_sheenColor.g, m_sheenColor.b);
    const QColor chosen = QColorDialog::getColor(initial, this, tr("Sheen Colour"));
    if (!chosen.isValid()) {
        return;
    }
    m_sheenColor = glm::vec3(static_cast<float>(chosen.redF()),
                             static_cast<float>(chosen.greenF()),
                             static_cast<float>(chosen.blueF()));
    updateColorButton(m_sheenColorBtn, m_sheenColor);
    applyChanges();
}

void MaterialEditorPanel::applyChanges() {
    if (m_currentIndex < 0 || !m_currentMaterial) {
        return;
    }

    // Create modified material
    quantiloom::Material modified = *m_currentMaterial;
    modified.baseColorFactor = m_baseColor;
    modified.metallicFactor = m_metallic;
    modified.roughnessFactor = m_roughness;
    modified.emissiveFactor = m_emissive;
    // The token only. Resolving it -- loading the spectrum, levelling it, and
    // deriving the colour the RGB half must use -- is the core's job, and doing
    // any of it here would be a second reading of the same rule.
    modified.emissiveCurveSource = m_emissiveCurveSource.toStdString();

    modified.transmission = m_transmission;
    modified.ior = m_ior;
    modified.dispersion = m_dispersion;
    modified.attenuationDistance = m_attenuationDistance;
    modified.attenuationColor = m_attenuationColor;

    modified.sheenColorFactor = m_sheenColor;
    modified.sheenRoughnessFactor = m_sheenRoughness;
    modified.specularFactor = m_specular;
    modified.specularColorFactor = m_specularColor;
    modified.anisotropyStrength = m_anisotropyStrength;
    modified.anisotropyRotation = m_anisotropyRotation;
    modified.clearcoatFactor = m_clearcoat;
    modified.clearcoatRoughnessFactor = m_clearcoatRoughness;
    modified.diffuseTransmissionFactor = m_diffuseTransmission;
    modified.diffuseTransmissionColorFactor = m_diffuseTransmissionColor;
    modified.alphaMode = static_cast<quantiloom::Material::AlphaMode>(m_alphaMode);
    modified.alphaCutoff = m_alphaCutoff;

    applyIrScalars(modified, m_irEmissivity, m_irTransmittance, m_irTemperature_K);

    // The scale and offset are material data and reach the shader with this
    // edit; the path only takes effect at the next scene load, which is what
    // the notice says.
    modified.temperatureTexturePath = m_temperatureTexture.toStdString();
    modified.temperatureScale = m_temperatureScale;
    modified.temperatureOffset = m_temperatureOffset;

    emit materialChanged(m_currentIndex, modified);
}

void MaterialEditorPanel::applyIrScalars(quantiloom::Material& material, float emissivity,
                                         float transmittance, float temperatureK) {
    if (emissivity <= 0.0f && transmittance <= 0.0f && temperatureK <= 0.0f) {
        return;
    }

    material.irEmissivityCurve.clear();
    material.irTransmittanceCurve.clear();
    material.irReflectanceCurve.clear();

    // Constant across the band: two points, one at each end of the thermal
    // range the renderer integrates.
    const float mwir_nm = 4000.0f;   // 4 um
    const float lwir_nm = 10000.0f;  // 10 um

    if (emissivity > 0.0f) {
        material.irEmissivityCurve.push_back({mwir_nm, emissivity});
        material.irEmissivityCurve.push_back({lwir_nm, emissivity});
    }

    if (transmittance > 0.0f) {
        material.irTransmittanceCurve.push_back({mwir_nm, transmittance});
        material.irTransmittanceCurve.push_back({lwir_nm, transmittance});
    }

    // Reflectance is what is left: Kirchhoff in thermal equilibrium says
    // epsilon + rho + tau = 1, so it is derived rather than given.
    const float reflectance = 1.0f - emissivity - transmittance;
    if (reflectance > 0.0f) {
        material.irReflectanceCurve.push_back({mwir_nm, reflectance});
        material.irReflectanceCurve.push_back({lwir_nm, reflectance});
    }

    material.irTemperature_K = temperatureK;
}
