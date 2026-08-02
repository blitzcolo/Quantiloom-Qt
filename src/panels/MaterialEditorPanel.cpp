/**
 * @file MaterialEditorPanel.cpp
 * @brief PBR material property editor implementation
 */

#include "MaterialEditorPanel.hpp"

#include "../ui/UiStyle.hpp"
#include "../ui/theme/ThemeManager.hpp"

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

#include <scene/Material.hpp>

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
    auto* emissiveLayout = new QHBoxLayout(emissiveGroup);

    auto createSpinBox = [this]() {
        auto* spin = new QDoubleSpinBox();
        spin->setRange(0.0, 100.0);
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

    // Kirchhoff's law validation label
    m_irKirchhoffLabel = new QLabel();
    bindStyle([this] { uistyle::applyHintStyle(m_irKirchhoffLabel); });
    irLayout->addRow(m_irKirchhoffLabel);

    mainLayout->addWidget(m_irGroup);
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

    m_emissive = material->emissiveFactor;
    {
        const QSignalBlocker r(m_emissiveR);
        const QSignalBlocker g(m_emissiveG);
        const QSignalBlocker b(m_emissiveB);
        m_emissiveR->setValue(m_emissive.r);
        m_emissiveG->setValue(m_emissive.g);
        m_emissiveB->setValue(m_emissive.b);
    }

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

    {
        const QSignalBlocker e(m_irEmissivitySpin);
        const QSignalBlocker t(m_irTransmittanceSpin);
        const QSignalBlocker k(m_irTemperatureSpin);
        m_irEmissivitySpin->setValue(static_cast<double>(m_irEmissivity));
        m_irTransmittanceSpin->setValue(static_cast<double>(m_irTransmittance));
        m_irTemperatureSpin->setValue(static_cast<double>(m_irTemperature_K));
    }

    updateSpectralCurveNotice();
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

    applyIrScalars(modified, m_irEmissivity, m_irTransmittance, m_irTemperature_K);

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
