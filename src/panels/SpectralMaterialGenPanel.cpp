/**
 * @file SpectralMaterialGenPanel.cpp
 * @brief Spectral material generator/modifier panel implementation
 */

#include "SpectralMaterialGenPanel.hpp"

#include "../ui/UiStyle.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextEdit>
#include <QTextStream>
#include <QDateTime>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <scene/Material.hpp>
#include <scene/Texture.hpp>
#include <scene/Scene.hpp>
#include <io/SpectralIO.hpp>
#include <SpectraForge.hpp>

#include <algorithm>
#include <cmath>

namespace {
// The span the generator's own range spinboxes may cover -- wider than the
// viewport's, because it also produces LWIR curves.
constexpr double kGenLambdaFloor = 100.0;
constexpr double kGenLambdaCeil  = 20000.0;
}  // namespace

SpectralMaterialGenPanel::SpectralMaterialGenPanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());
    setupUi();
    // Load default preset
    onMaterialTypeChanged(0);
}

void SpectralMaterialGenPanel::setCurrentMaterialIndex(int index) {
    m_currentMaterialIndex = index;
    if (m_targetMaterialSpin && index >= 0)
        m_targetMaterialSpin->setValue(index);
}

void SpectralMaterialGenPanel::setScene(const quantiloom::Scene* scene) {
    m_scene = scene;
}

QString SpectralMaterialGenPanel::panelTitle() const {
    return tr("Spectral Material Generator");
}

void SpectralMaterialGenPanel::retranslateUi() {
    PanelBase::retranslateUi();
}

// ============================================================================
// UI Setup
// ============================================================================

void SpectralMaterialGenPanel::setupUi() {
    // No scroll area of its own any more: the shell puts every panel inside
    // one, and two nested scroll areas produce two sets of scrollbars.
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(4);

    // --- Auto IR Generation group (SpectraForge) ---
    {
        auto* group = new QGroupBox(this);
        auto* form = new QFormLayout(group);
        form->setContentsMargins(6, 10, 6, 6);

        // Mode selection
        auto* modeRow = new QHBoxLayout();
        m_autoIRSingleRadio = new QRadioButton(group);
        m_autoIRAllRadio = new QRadioButton(group);
        m_autoIRSingleRadio->setChecked(true);
        modeRow->addWidget(m_autoIRSingleRadio);
        modeRow->addWidget(m_autoIRAllRadio);
        auto* modeCaption = new QLabel(group);
        form->addRow(modeCaption, modeRow);

        // Cluster count
        m_clusterCountSpin = new QSpinBox();
        m_clusterCountSpin->setRange(1, 12);
        m_clusterCountSpin->setValue(5);
        auto* clusterCaption = new QLabel(group);
        form->addRow(clusterCaption, m_clusterCountSpin);

        // Temperature
        m_temperatureSpin = new QDoubleSpinBox();
        m_temperatureSpin->setRange(100.0, 5000.0);
        m_temperatureSpin->setValue(300.0);
        auto* tempCaption = new QLabel(group);
        form->addRow(tempCaption, m_temperatureSpin);

        // Overwrite existing
        m_overwriteCheck = new QCheckBox(group);
        m_overwriteCheck->setChecked(false);
        form->addRow(m_overwriteCheck);

        // Generate button
        auto* generateBtn = new QPushButton(group);
        form->addRow(generateBtn);

        // Result text
        m_autoIRResultText = new QTextEdit();
        m_autoIRResultText->setReadOnly(true);
        m_autoIRResultText->setMaximumHeight(100);
        form->addRow(m_autoIRResultText);

        bindText([this, group, modeCaption, clusterCaption, tempCaption, generateBtn] {
            group->setTitle(tr("Auto IR Generation (SpectraForge)"));
            m_autoIRSingleRadio->setText(tr("Current Material"));
            m_autoIRAllRadio->setText(tr("All Materials"));
            modeCaption->setText(tr("Mode:"));
            clusterCaption->setText(tr("Clusters (K):"));
            m_clusterCountSpin->setToolTip(tr("K-means cluster count for texture color analysis"));
            tempCaption->setText(tr("Cluster temperature:"));
            m_temperatureSpin->setSuffix(tr(" K"));
            m_temperatureSpin->setToolTip(
                tr("Surface temperature assigned to the materials generated from each cluster"));
            m_overwriteCheck->setText(tr("Overwrite existing IR data"));
            generateBtn->setText(tr("Generate IR Materials"));
            m_autoIRResultText->setPlaceholderText(tr("Results will appear here..."));
        });

        mainLayout->addWidget(group);

        connect(generateBtn, &QPushButton::clicked,
                this, &SpectralMaterialGenPanel::onAutoIRGenerate);
    }

    // --- Material Type group ---
    {
        auto* group = new QGroupBox(this);
        auto* form = new QFormLayout(group);
        form->setContentsMargins(6, 10, 6, 6);

        m_materialTypeCombo = new QComboBox();
        m_materialTypeCombo->addItems({QString(), QString(), QString()});
        auto* typeCaption = new QLabel(group);
        form->addRow(typeCaption, m_materialTypeCombo);

        m_roughnessSpin = new QDoubleSpinBox();
        m_roughnessSpin->setRange(0.0, 1.0);
        m_roughnessSpin->setSingleStep(0.01);
        m_roughnessSpin->setValue(0.5);
        auto* roughCaption = new QLabel(group);
        form->addRow(roughCaption, m_roughnessSpin);

        bindText([this, group, typeCaption, roughCaption] {
            group->setTitle(tr("Material Type"));
            typeCaption->setText(tr("Type:"));
            roughCaption->setText(tr("Roughness:"));
            // Item text rather than a rebuilt list: the current index is the
            // panel's state and must survive a language switch.
            m_materialTypeCombo->setItemText(0, tr("Conductor"));
            m_materialTypeCombo->setItemText(1, tr("Dielectric"));
            m_materialTypeCombo->setItemText(2, tr("Semiconductor"));
        });

        mainLayout->addWidget(group);

        connect(m_materialTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &SpectralMaterialGenPanel::onMaterialTypeChanged);
    }

    // --- Wavelength Range group ---
    {
        auto* group = new QGroupBox(this);
        auto* form = new QFormLayout(group);
        form->setContentsMargins(6, 10, 6, 6);

        m_lambdaStartSpin = new QDoubleSpinBox();
        m_lambdaStartSpin->setRange(kGenLambdaFloor, kGenLambdaCeil);
        m_lambdaStartSpin->setValue(380.0);
        auto* startCaption = new QLabel(group);
        form->addRow(startCaption, m_lambdaStartSpin);

        m_lambdaEndSpin = new QDoubleSpinBox();
        m_lambdaEndSpin->setRange(kGenLambdaFloor, kGenLambdaCeil);
        m_lambdaEndSpin->setValue(780.0);
        auto* endCaption = new QLabel(group);
        form->addRow(endCaption, m_lambdaEndSpin);

        m_outputStepsSpin = new QSpinBox();
        m_outputStepsSpin->setRange(8, 512);
        m_outputStepsSpin->setValue(64);
        auto* stepsCaption = new QLabel(group);
        form->addRow(stepsCaption, m_outputStepsSpin);

        // Interpolation used to give up silently on an inverted range, leaving
        // the last good curve on the chart with nothing to say why it stopped
        // following the spinboxes.
        m_rangeWarning = new QLabel(group);
        m_rangeWarning->setWordWrap(true);
        m_rangeWarning->setVisible(false);
        bindStyle([this] { uistyle::applyNoticeStyle(m_rangeWarning); });
        form->addRow(m_rangeWarning);

        bindText([this, group, startCaption, endCaption, stepsCaption] {
            group->setTitle(tr("Wavelength Range"));
            startCaption->setText(tr("Start:"));
            endCaption->setText(tr("End:"));
            stepsCaption->setText(tr("Output Steps:"));
            m_lambdaStartSpin->setSuffix(tr(" nm"));
            m_lambdaEndSpin->setSuffix(tr(" nm"));
            m_rangeWarning->setText(
                tr("Start must be below End, and Output Steps at least 2, "
                   "for a curve to be generated."));
        });

        mainLayout->addWidget(group);

        connect(m_lambdaStartSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &SpectralMaterialGenPanel::onAnchorDataChanged);
        connect(m_lambdaEndSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &SpectralMaterialGenPanel::onAnchorDataChanged);
        connect(m_outputStepsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { onAnchorDataChanged(); });
    }

    // --- Anchor Points group ---
    {
        auto* group = new QGroupBox(this);
        auto* vlay = new QVBoxLayout(group);
        vlay->setContentsMargins(6, 10, 6, 6);

        m_anchorTable = new QTableWidget(0, 3);
        m_anchorTable->setHorizontalHeaderLabels({
            QStringLiteral("\316\273 (nm)"),   // λ (nm)
            QStringLiteral("n"),
            QStringLiteral("k")
        });
        m_anchorTable->horizontalHeader()->setStretchLastSection(true);
        m_anchorTable->setMinimumHeight(120);
        vlay->addWidget(m_anchorTable);

        auto* btnRow = new QHBoxLayout();
        auto* addBtn = new QPushButton(group);
        auto* removeBtn = new QPushButton(group);
        auto* loadCsvBtn = new QPushButton(group);
        auto* loadYamlBtn = new QPushButton(group);
        btnRow->addWidget(addBtn);
        btnRow->addWidget(removeBtn);
        btnRow->addWidget(loadCsvBtn);
        btnRow->addWidget(loadYamlBtn);
        vlay->addLayout(btnRow);

        auto* interpRow = new QHBoxLayout();
        auto* interpCaption = new QLabel(group);
        interpRow->addWidget(interpCaption);
        m_interpCombo = new QComboBox();
        m_interpCombo->addItems({QString(), QStringLiteral("PCHIP")});
        m_interpCombo->setCurrentIndex(1);  // Default PCHIP
        interpRow->addWidget(m_interpCombo);
        vlay->addLayout(interpRow);

        bindText([this, group, addBtn, removeBtn, loadCsvBtn, loadYamlBtn, interpCaption] {
            group->setTitle(tr("Anchor Points"));
            addBtn->setText(tr("Add Point"));
            removeBtn->setText(tr("Remove Point"));
            loadCsvBtn->setText(tr("Load CSV"));
            loadYamlBtn->setText(tr("Load YAML"));
            interpCaption->setText(tr("Interpolation:"));
            // PCHIP stays Latin by glossary decision; "Linear" does not.
            m_interpCombo->setItemText(0, tr("Linear"));
        });

        mainLayout->addWidget(group);

        connect(addBtn, &QPushButton::clicked, this, &SpectralMaterialGenPanel::onAddAnchorPoint);
        connect(removeBtn, &QPushButton::clicked, this, &SpectralMaterialGenPanel::onRemoveAnchorPoint);
        connect(loadCsvBtn, &QPushButton::clicked, this, &SpectralMaterialGenPanel::onLoadCSV);
        connect(loadYamlBtn, &QPushButton::clicked, this, &SpectralMaterialGenPanel::onLoadYAML);
        connect(m_anchorTable, &QTableWidget::cellChanged,
                this, &SpectralMaterialGenPanel::onAnchorDataChanged);
        connect(m_interpCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { onAnchorDataChanged(); });
    }

    // --- Preview Chart group ---
    {
        m_previewGroup = new QGroupBox(this);
        bindText([this] { m_previewGroup->setTitle(tr("Preview")); });
        auto* vlay = new QVBoxLayout(m_previewGroup);
        vlay->setContentsMargins(2, 10, 2, 2);

        auto* chart = new QChart();
        bindText([chart] { chart->setTitle(tr("Spectral Curves")); });
        chart->setAnimationOptions(QChart::NoAnimation);
        chart->legend()->setVisible(true);

        m_chartView = new QChartView(chart);
        m_chartView->setRenderHint(QPainter::Antialiasing);
        m_chartView->setMinimumHeight(200);
        vlay->addWidget(m_chartView);

        // The chart paints itself, so it does not follow the shell style sheet
        // the way the ordinary widgets do -- it has to be repainted by hand on
        // every theme switch.
        bindStyle([this] { applyChartTheme(); });

        mainLayout->addWidget(m_previewGroup, 1);
    }

    // --- Actions group ---
    {
        auto* group = new QGroupBox(this);
        auto* form = new QFormLayout(group);
        form->setContentsMargins(6, 10, 6, 6);

        auto* saveCsvBtn = new QPushButton(group);
        form->addRow(saveCsvBtn);

        m_targetMaterialSpin = new QSpinBox();
        m_targetMaterialSpin->setRange(0, 255);
        m_targetMaterialSpin->setValue(0);
        auto* targetCaption = new QLabel(group);
        form->addRow(targetCaption, m_targetMaterialSpin);

        auto* applyBtn = new QPushButton(group);
        form->addRow(applyBtn);

        bindText([this, group, saveCsvBtn, targetCaption, applyBtn] {
            group->setTitle(tr("Actions"));
            saveCsvBtn->setText(tr("Save CSV"));
            targetCaption->setText(tr("Target Material Index:"));
            applyBtn->setText(tr("Apply to Material"));
        });

        mainLayout->addWidget(group);

        connect(saveCsvBtn, &QPushButton::clicked, this, &SpectralMaterialGenPanel::onSaveCSV);
        connect(applyBtn, &QPushButton::clicked, this, &SpectralMaterialGenPanel::onApplyToMaterial);
    }

    mainLayout->addStretch();
}

// ============================================================================
// Auto IR Generation (SpectraForge)
// ============================================================================

void SpectralMaterialGenPanel::onAutoIRGenerate() {
    const auto* scene = m_scene;
    if (!scene || scene->materials.empty()) {
        QMessageBox::warning(this, tr("Error"), tr("No scene loaded or no materials in scene."));
        return;
    }

    auto clusterCount = static_cast<quantiloom::u32>(m_clusterCountSpin->value());
    auto temperature = static_cast<quantiloom::f32>(m_temperatureSpin->value());
    bool overwrite = m_overwriteCheck->isChecked();

    m_autoIRResultText->clear();

    if (m_autoIRSingleRadio->isChecked()) {
        // Single material mode
        int idx = m_currentMaterialIndex;
        if (idx < 0 || idx >= static_cast<int>(scene->materials.size())) {
            QMessageBox::warning(this, tr("Error"),
                tr("No material selected. Select a material in the Scene Tree first."));
            return;
        }

        quantiloom::Material mat = scene->materials[static_cast<quantiloom::u32>(idx)];

        if (!overwrite && mat.HasIRData()) {
            m_autoIRResultText->append(
                tr("Material[%1] '%2' already has IR data, skipped.")
                    .arg(idx)
                    .arg(QString::fromStdString(mat.name)));
            return;
        }

        const quantiloom::Texture* tex = nullptr;
        if (mat.baseColorTextureIndex >= 0 &&
            static_cast<quantiloom::u32>(mat.baseColorTextureIndex) < scene->textures.size()) {
            tex = &scene->textures[static_cast<quantiloom::u32>(mat.baseColorTextureIndex)];
        }

        bool ok = spectraforge::SpectraForge::ProcessSingle(mat, tex, temperature, clusterCount);
        if (ok) {
            emit materialChanged(idx, mat);
            m_autoIRResultText->append(
                tr("Material[%1] '%2' -> IR generated (T=%3 K)")
                    .arg(idx)
                    .arg(QString::fromStdString(mat.name))
                    .arg(temperature, 0, 'f', 1));
        } else {
            m_autoIRResultText->append(tr("Failed to process material[%1]").arg(idx));
        }
    } else {
        // All materials mode
        spectraforge::ForgeConfig config;
        config.clusterCount = clusterCount;
        config.defaultTemperature_K = temperature;
        config.overwriteExisting = overwrite;
        config.verbose = true;

        quantiloom::Vector<quantiloom::Material> outMats;
        auto result = spectraforge::SpectraForge::Process(*scene, outMats, config);

        // Apply each modified material
        for (quantiloom::u32 i = 0; i < outMats.size(); ++i) {
            emit materialChanged(static_cast<int>(i), outMats[i]);
        }

        // Display results
        m_autoIRResultText->append(
            tr("Processed: %1/%2, Skipped: %3")
                .arg(result.materialsProcessed)
                .arg(result.totalMaterials)
                .arg(result.materialsSkipped));

        for (const auto& assignment : result.assignments) {
            m_autoIRResultText->append(QString::fromStdString(assignment));
        }
    }
}

// ============================================================================
// Material Type Presets
// ============================================================================

void SpectralMaterialGenPanel::onMaterialTypeChanged(int index) {
    // Block signals to avoid triggering reinterpolate per-cell
    m_anchorTable->blockSignals(true);
    m_anchorTable->setRowCount(0);

    struct Anchor { float lambda, n, k; };
    std::vector<Anchor> presets;

    switch (index) {
    case 0: // Conductor (Aluminum)
        presets = {{380, 0.04f, 1.66f}, {500, 0.13f, 2.88f},
                   {700, 0.14f, 4.26f}, {1000, 0.26f, 6.97f}};
        m_roughnessSpin->setValue(0.35);
        break;
    case 1: // Dielectric (Glass)
        presets = {{380, 1.53f, 0.0f}, {550, 1.50f, 0.0f}, {780, 1.49f, 0.0f}};
        m_roughnessSpin->setValue(0.05);
        break;
    case 2: // Semiconductor (Silicon)
        presets = {{380, 5.57f, 2.98f}, {500, 4.30f, 0.07f},
                   {700, 3.78f, 0.01f}, {1000, 3.57f, 0.0f}};
        m_roughnessSpin->setValue(0.5);
        break;
    }

    for (auto& p : presets) {
        int row = m_anchorTable->rowCount();
        m_anchorTable->insertRow(row);
        m_anchorTable->setItem(row, 0, new QTableWidgetItem(QString::number(p.lambda, 'f', 1)));
        m_anchorTable->setItem(row, 1, new QTableWidgetItem(QString::number(p.n, 'f', 3)));
        m_anchorTable->setItem(row, 2, new QTableWidgetItem(QString::number(p.k, 'f', 3)));
    }

    m_anchorTable->blockSignals(false);
    reinterpolate();
}

// ============================================================================
// Anchor Point Management
// ============================================================================

void SpectralMaterialGenPanel::onAddAnchorPoint() {
    int row = m_anchorTable->rowCount();
    m_anchorTable->blockSignals(true);
    m_anchorTable->insertRow(row);
    m_anchorTable->setItem(row, 0, new QTableWidgetItem("550.0"));
    m_anchorTable->setItem(row, 1, new QTableWidgetItem("1.500"));
    m_anchorTable->setItem(row, 2, new QTableWidgetItem("0.000"));
    m_anchorTable->blockSignals(false);
    reinterpolate();
}

void SpectralMaterialGenPanel::onRemoveAnchorPoint() {
    int row = m_anchorTable->currentRow();
    if (row >= 0) {
        m_anchorTable->removeRow(row);
        reinterpolate();
    }
}

void SpectralMaterialGenPanel::onAnchorDataChanged() {
    reinterpolate();
}

// ============================================================================
// Interpolation Core
// ============================================================================

void SpectralMaterialGenPanel::reinterpolate() {
    // 1. Read anchor points from table
    int rows = m_anchorTable->rowCount();
    struct Point { float lambda, n, k; };
    std::vector<Point> pts;
    pts.reserve(rows);

    for (int r = 0; r < rows; ++r) {
        auto* li = m_anchorTable->item(r, 0);
        auto* ni = m_anchorTable->item(r, 1);
        auto* ki = m_anchorTable->item(r, 2);
        if (!li || !ni || !ki) continue;

        bool okL, okN, okK;
        float l = li->text().toFloat(&okL);
        float n = ni->text().toFloat(&okN);
        float k = ki->text().toFloat(&okK);
        if (okL && okN && okK)
            pts.push_back({l, n, k});
    }

    // 2. Sort by wavelength
    std::sort(pts.begin(), pts.end(),
              [](const Point& a, const Point& b) { return a.lambda < b.lambda; });

    // 3. Need at least 2 points
    if (pts.size() < 2) {
        m_interpolatedCRI = quantiloom::ComplexRefractiveIndex{};
        updateChart();
        return;
    }

    // 4. Generate output wavelength grid
    float start = static_cast<float>(m_lambdaStartSpin->value());
    float end   = static_cast<float>(m_lambdaEndSpin->value());
    int   steps = m_outputStepsSpin->value();
    const bool rangeUsable = (start < end) && (steps >= 2);
    if (m_rangeWarning) {
        m_rangeWarning->setVisible(!rangeUsable);
    }
    if (!rangeUsable) {
        m_interpolatedCRI = quantiloom::ComplexRefractiveIndex{};
        updateChart();
        return;
    }

    std::vector<float> xq(steps);
    for (int i = 0; i < steps; ++i)
        xq[i] = start + (end - start) * i / (steps - 1);

    // 5. Separate anchor data
    std::vector<float> lambdas, ns, ks;
    lambdas.reserve(pts.size());
    ns.reserve(pts.size());
    ks.reserve(pts.size());
    for (auto& p : pts) {
        lambdas.push_back(p.lambda);
        ns.push_back(p.n);
        ks.push_back(p.k);
    }

    // 6. Interpolate
    std::vector<float> nInterp, kInterp;
    if (m_interpCombo->currentIndex() == 0) {
        // Linear: use CRI's built-in linear interpolation by sampling
        quantiloom::ComplexRefractiveIndex tempCRI;
        tempCRI.wavelengths_nm.assign(lambdas.begin(), lambdas.end());
        tempCRI.n.assign(ns.begin(), ns.end());
        tempCRI.k.assign(ks.begin(), ks.end());

        nInterp.resize(steps);
        kInterp.resize(steps);
        for (int i = 0; i < steps; ++i) {
            auto [nv, kv] = tempCRI.Evaluate(xq[i]);
            nInterp[i] = nv;
            kInterp[i] = kv;
        }
    } else {
        // PCHIP
        nInterp = pchipInterpolate(lambdas, ns, xq);
        kInterp = pchipInterpolate(lambdas, ks, xq);
    }

    // 7. Fill interpolated CRI
    m_interpolatedCRI.wavelengths_nm.assign(xq.begin(), xq.end());
    m_interpolatedCRI.n.assign(nInterp.begin(), nInterp.end());
    m_interpolatedCRI.k.assign(kInterp.begin(), kInterp.end());

    updateChart();
}

// ============================================================================
// PCHIP Interpolation (Fritsch-Carlson monotone cubic Hermite)
// ============================================================================

std::vector<float> SpectralMaterialGenPanel::pchipInterpolate(
    const std::vector<float>& xs, const std::vector<float>& ys,
    const std::vector<float>& xq)
{
    const size_t n = xs.size();
    if (n < 2) return std::vector<float>(xq.size(), ys.empty() ? 0.0f : ys[0]);

    // Compute secants
    std::vector<float> delta(n - 1);
    for (size_t i = 0; i < n - 1; ++i)
        delta[i] = (ys[i + 1] - ys[i]) / (xs[i + 1] - xs[i]);

    // Compute derivatives
    std::vector<float> d(n, 0.0f);

    if (n == 2) {
        d[0] = delta[0];
        d[1] = delta[0];
    } else {
        // Endpoints: one-sided differences
        d[0] = delta[0];
        d[n - 1] = delta[n - 2];

        // Interior points
        for (size_t i = 1; i < n - 1; ++i) {
            if (delta[i - 1] * delta[i] > 0.0f) {
                // Harmonic mean (Fritsch-Carlson)
                d[i] = 2.0f / (1.0f / delta[i - 1] + 1.0f / delta[i]);
            } else {
                d[i] = 0.0f;
            }
        }
    }

    // Evaluate at query points using Hermite basis
    std::vector<float> result(xq.size());
    size_t seg = 0;

    for (size_t q = 0; q < xq.size(); ++q) {
        float x = xq[q];

        // Clamp to range
        if (x <= xs[0]) { result[q] = ys[0]; continue; }
        if (x >= xs[n - 1]) { result[q] = ys[n - 1]; continue; }

        // Find interval (advance segment)
        while (seg < n - 2 && x > xs[seg + 1]) ++seg;

        float h = xs[seg + 1] - xs[seg];
        float t = (x - xs[seg]) / h;
        float t2 = t * t;
        float t3 = t2 * t;

        // Hermite basis functions
        float h00 = 2 * t3 - 3 * t2 + 1;
        float h10 = t3 - 2 * t2 + t;
        float h01 = -2 * t3 + 3 * t2;
        float h11 = t3 - t2;

        result[q] = h00 * ys[seg] + h10 * h * d[seg]
                  + h01 * ys[seg + 1] + h11 * h * d[seg + 1];
    }

    return result;
}

// ============================================================================
// Chart Update
// ============================================================================

void SpectralMaterialGenPanel::updateChart() {
    auto* chart = m_chartView->chart();
    chart->removeAllSeries();

    // Remove old axes
    const auto axes = chart->axes();
    for (auto* ax : axes)
        chart->removeAxis(ax);

    if (!m_interpolatedCRI.IsValid()) return;

    // R0, n and k are the conventional symbols for these quantities in every
    // language; they are kept verbatim by decision, and the glossary records
    // that. Only the descriptive axis titles are translated.
    auto* seriesR0 = new QLineSeries();
    seriesR0->setName(QStringLiteral("R0"));

    auto* seriesN = new QLineSeries();
    seriesN->setName(QStringLiteral("n"));

    auto* seriesK = new QLineSeries();
    seriesK->setName(QStringLiteral("k"));

    float nMax = 0, kMax = 0;
    const auto& wl = m_interpolatedCRI.wavelengths_nm;
    const auto& nVals = m_interpolatedCRI.n;
    const auto& kVals = m_interpolatedCRI.k;

    for (size_t i = 0; i < wl.size(); ++i) {
        float lambda = wl[i];
        float nv = nVals[i];
        float kv = kVals[i];
        float r0 = m_interpolatedCRI.FresnelR0(lambda);

        seriesR0->append(lambda, r0);
        seriesN->append(lambda, nv);
        seriesK->append(lambda, kv);

        nMax = std::max(nMax, nv);
        kMax = std::max(kMax, kv);
    }

    chart->addSeries(seriesR0);
    chart->addSeries(seriesN);
    chart->addSeries(seriesK);

    // X axis (wavelength)
    auto* axisX = new QValueAxis();
    axisX->setTitleText(tr("Wavelength (nm)"));
    axisX->setRange(wl.front(), wl.back());
    chart->addAxis(axisX, Qt::AlignBottom);
    seriesR0->attachAxis(axisX);
    seriesN->attachAxis(axisX);
    seriesK->attachAxis(axisX);

    // Y axis left (R0 and n)
    auto* axisYLeft = new QValueAxis();
    axisYLeft->setTitleText(QStringLiteral("R0 / n"));
    axisYLeft->setRange(0.0, std::max(1.0f, nMax * 1.1f));
    chart->addAxis(axisYLeft, Qt::AlignLeft);
    seriesR0->attachAxis(axisYLeft);
    seriesN->attachAxis(axisYLeft);

    // Y axis right (k)
    auto* axisYRight = new QValueAxis();
    axisYRight->setTitleText(QStringLiteral("k"));
    axisYRight->setRange(0.0, std::max(0.1f, kMax * 1.1f));
    chart->addAxis(axisYRight, Qt::AlignRight);
    seriesK->attachAxis(axisYRight);

    // The series and axes were just recreated, so they carry Qt's defaults
    // until this paints the current theme onto them.
    applyChartTheme();
}

void SpectralMaterialGenPanel::applyChartTheme() {
    if (!m_chartView) {
        return;
    }
    auto* chart = m_chartView->chart();

    // QtCharts draws with its own defaults, which are a light chart however
    // dark the theme is. Everything below comes from the palette, and is
    // assigned absolutely rather than derived from the chart's current colours,
    // so re-running it on each theme switch is idempotent.
    const QColor text    = palette().color(QPalette::WindowText);
    const QColor base    = palette().color(QPalette::Base);
    const QColor window  = palette().color(QPalette::Window);
    const bool darkTheme = window.lightnessF() < 0.5;

    chart->setBackgroundBrush(base);
    chart->setBackgroundPen(Qt::NoPen);
    chart->setPlotAreaBackgroundBrush(base);
    chart->setTitleBrush(text);
    if (chart->legend()) {
        chart->legend()->setLabelColor(text);
        chart->legend()->setBackgroundVisible(false);
    }

    // Grid lines read as texture, not content: a low-contrast blend of the
    // text colour into the background.
    QColor grid = text;
    grid.setAlphaF(0.25);

    const auto axes = chart->axes();
    for (auto* axis : axes) {
        axis->setLabelsColor(text);
        axis->setTitleBrush(text);
        axis->setLinePenColor(text);
        axis->setGridLineColor(grid);
    }

    // Three hues that stay legible against both a near-white and a near-black
    // plot area; the dark-theme set is lightened rather than a different hue,
    // so the curves keep their identity across themes.
    const QColor r0Colour = darkTheme ? QColor(0xFF, 0x7A, 0x7A) : QColor(0xC0, 0x21, 0x21);
    const QColor nColour  = darkTheme ? QColor(0x7A, 0xB8, 0xFF) : QColor(0x1A, 0x4F, 0xC4);
    const QColor kColour  = darkTheme ? QColor(0x74, 0xD6, 0x9C) : QColor(0x1B, 0x7A, 0x43);

    const auto seriesList = chart->series();
    for (auto* series : seriesList) {
        auto* line = qobject_cast<QLineSeries*>(series);
        if (!line) continue;
        if (line->name() == QStringLiteral("R0"))     line->setColor(r0Colour);
        else if (line->name() == QStringLiteral("n")) line->setColor(nColour);
        else if (line->name() == QStringLiteral("k")) line->setColor(kColour);
    }
}

// ============================================================================
// CSV Load / Save
// ============================================================================

void SpectralMaterialGenPanel::onLoadCSV() {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Load Spectral CSV"), QString(),
        tr("CSV Files (*.csv);;All Files (*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot open file: %1").arg(path));
        return;
    }

    m_anchorTable->blockSignals(true);
    m_anchorTable->setRowCount(0);

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // Parse metadata comments
        if (line.startsWith('#')) {
            if (line.contains("material_type")) {
                QString val = line.section('=', 1).trimmed();
                if (val == "conductor") m_materialTypeCombo->setCurrentIndex(0);
                else if (val == "dielectric") m_materialTypeCombo->setCurrentIndex(1);
                else if (val == "semiconductor") m_materialTypeCombo->setCurrentIndex(2);
            } else if (line.contains("roughness")) {
                bool ok;
                double v = line.section('=', 1).trimmed().toDouble(&ok);
                if (ok) m_roughnessSpin->setValue(v);
            } else if (line.contains("interpolation")) {
                QString val = line.section('=', 1).trimmed().toLower();
                m_interpCombo->setCurrentIndex(val == "pchip" ? 1 : 0);
            }
            continue;
        }

        // Skip header line
        if (line.startsWith("wavelength")) continue;

        // Parse data: wavelength_nm,n,k
        QStringList parts = line.split(',');
        if (parts.size() >= 3) {
            int row = m_anchorTable->rowCount();
            m_anchorTable->insertRow(row);
            m_anchorTable->setItem(row, 0, new QTableWidgetItem(parts[0].trimmed()));
            m_anchorTable->setItem(row, 1, new QTableWidgetItem(parts[1].trimmed()));
            m_anchorTable->setItem(row, 2, new QTableWidgetItem(parts[2].trimmed()));
        }
    }

    m_anchorTable->blockSignals(false);
    reinterpolate();
}

void SpectralMaterialGenPanel::onSaveCSV() {
    if (!m_interpolatedCRI.IsValid()) {
        QMessageBox::information(this, tr("Info"), tr("No data to save. Add anchor points first."));
        return;
    }

    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Spectral CSV"), QString(),
        tr("CSV Files (*.csv);;All Files (*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot write file: %1").arg(path));
        return;
    }

    QTextStream out(&file);

    // Metadata header
    static const char* typeNames[] = {"conductor", "dielectric", "semiconductor"};
    out << "# Quantiloom Spectral Material\n";
    out << "# material_type = " << typeNames[m_materialTypeCombo->currentIndex()] << "\n";
    out << "# roughness = " << QString::number(m_roughnessSpin->value(), 'f', 3) << "\n";
    out << "# interpolation = " << (m_interpCombo->currentIndex() == 1 ? "pchip" : "linear") << "\n";
    out << "# generated = " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "wavelength_nm,n,k\n";

    // Write interpolated data
    const auto& wl = m_interpolatedCRI.wavelengths_nm;
    const auto& nVals = m_interpolatedCRI.n;
    const auto& kVals = m_interpolatedCRI.k;

    for (size_t i = 0; i < wl.size(); ++i) {
        out << QString::number(wl[i], 'f', 3) << ","
            << QString::number(nVals[i], 'f', 6) << ","
            << QString::number(kVals[i], 'f', 6) << "\n";
    }
}

// ============================================================================
// YAML Load (RefractiveIndex.info format)
// ============================================================================

void SpectralMaterialGenPanel::onLoadYAML() {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Load RefractiveIndex.info YAML"), QString(),
        tr("YAML Files (*.yml *.yaml);;All Files (*)"));
    if (path.isEmpty()) return;

    auto result = quantiloom::SpectralIO::LoadRefractiveIndexYAML(
        std::filesystem::path(path.toStdString()));

    if (!result.has_value()) {
        QMessageBox::warning(this, tr("Error"),
            tr("Failed to load YAML: %1").arg(QString::fromStdString(result.error())));
        return;
    }

    const auto& cri = result.value();
    if (cri.wavelengths_nm.empty()) {
        QMessageBox::warning(this, tr("Error"), tr("YAML file contains no spectral data."));
        return;
    }

    // Fill anchor table from loaded CRI
    m_anchorTable->blockSignals(true);
    m_anchorTable->setRowCount(0);

    for (size_t i = 0; i < cri.wavelengths_nm.size(); ++i) {
        int row = m_anchorTable->rowCount();
        m_anchorTable->insertRow(row);
        m_anchorTable->setItem(row, 0,
            new QTableWidgetItem(QString::number(cri.wavelengths_nm[i], 'f', 1)));
        m_anchorTable->setItem(row, 1,
            new QTableWidgetItem(QString::number(cri.n[i], 'f', 4)));
        m_anchorTable->setItem(row, 2,
            new QTableWidgetItem(QString::number(cri.k[i], 'f', 4)));
    }

    // Auto-detect material type based on k values
    if (!cri.k.empty()) {
        float avgK = 0;
        for (auto kv : cri.k) avgK += kv;
        avgK /= static_cast<float>(cri.k.size());
        if (avgK > 1.0f) m_materialTypeCombo->setCurrentIndex(0);       // Conductor
        else if (avgK < 0.01f) m_materialTypeCombo->setCurrentIndex(1);  // Dielectric
        else m_materialTypeCombo->setCurrentIndex(2);                     // Semiconductor
    }

    // Update wavelength range from data
    m_lambdaStartSpin->setValue(cri.wavelengths_nm.front());
    m_lambdaEndSpin->setValue(cri.wavelengths_nm.back());

    m_anchorTable->blockSignals(false);
    reinterpolate();
}

// ============================================================================
// Apply to Material
// ============================================================================

void SpectralMaterialGenPanel::onApplyToMaterial() {
    if (!m_interpolatedCRI.IsValid()) {
        QMessageBox::information(this, tr("Info"),
            tr("No interpolated data. Add anchor points first."));
        return;
    }

    quantiloom::Material mat{};
    applyToMaterialStruct(mat);

    // The (n,k) curves have to reach the GPU before the material can point at
    // them, and that upload belongs to the shell.
    emit materialWithCriChanged(m_targetMaterialSpin->value(), mat, m_interpolatedCRI);
}

void SpectralMaterialGenPanel::applyToMaterialStruct(quantiloom::Material& mat) {
    mat.roughnessFactor = static_cast<float>(m_roughnessSpin->value());
    mat.metallicFactor = (m_materialTypeCombo->currentIndex() == 0) ? 1.0f : 0.0f;

    // Build R0(λ) reflectance curve from n,k
    mat.irReflectanceCurve.clear();
    const auto& wl = m_interpolatedCRI.wavelengths_nm;
    for (size_t i = 0; i < wl.size(); ++i) {
        float lambda = wl[i];
        float r0 = m_interpolatedCRI.FresnelR0(lambda);
        mat.irReflectanceCurve.push_back({lambda, r0});
    }

    mat.spectralSource = quantiloom::Material::SpectralSource::Procedural;
}

