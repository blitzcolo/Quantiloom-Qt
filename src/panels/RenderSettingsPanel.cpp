/**
 * @file RenderSettingsPanel.cpp
 * @brief Render settings panel implementation
 */

#include "RenderSettingsPanel.hpp"

#include <algorithm>

#include "../ui/ModeCatalog.hpp"
#include "../ui/UiStyle.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QFormLayout>
#include <QSignalBlocker>

RenderSettingsPanel::RenderSettingsPanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());
    setupUi();
}

QString RenderSettingsPanel::panelTitle() const {
    return tr("Render");
}

void RenderSettingsPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // --- status ---------------------------------------------------------
    auto* statusGroup = new QGroupBox(this);
    auto* statusLayout = new QFormLayout(statusGroup);

    m_sampleCountLabel = new QLabel(QStringLiteral("0"));
    bindStyle([this] { uistyle::applyHeadingStyle(m_sampleCountLabel); });
    auto* samplesCaption = new QLabel(statusGroup);
    bindText([statusGroup, samplesCaption] {
        statusGroup->setTitle(tr("Status"));
        samplesCaption->setText(tr("Accumulated samples:"));
    });
    statusLayout->addRow(samplesCaption, m_sampleCountLabel);

    mainLayout->addWidget(statusGroup);

    // --- quality --------------------------------------------------------
    auto* qualityGroup = new QGroupBox(this);
    auto* qualityLayout = new QFormLayout(qualityGroup);

    // Preset names come from the same catalogue as Render ▸ Quality, so the
    // two lists cannot drift apart.
    m_sppPreset = new QComboBox();
    for (const catalog::QualityPreset& preset : catalog::qualityPresets()) {
        m_sppPreset->addItem(QString(), preset.spp);
    }
    m_sppPreset->addItem(QString(), -1);   // custom
    // By value: the catalogue's order is its own business.
    m_sppPreset->setCurrentIndex(std::max(0, m_sppPreset->findData(4)));
    connect(m_sppPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RenderSettingsPanel::onSppPresetChanged);
    auto* targetCaption = new QLabel(qualityGroup);
    qualityLayout->addRow(targetCaption, m_sppPreset);

    m_customSpp = new QSpinBox();
    m_customSpp->setRange(1, 65536);
    m_customSpp->setValue(4);
    m_customSpp->setSuffix(QStringLiteral(" spp"));   // unit symbol, not translated
    // Applied on commit: typing 1024 used to set a target of 1, 10, 102 and
    // then 1024, restarting the render's timing at each.
    m_customSpp->setKeyboardTracking(false);
    m_customSpp->setEnabled(false);
    connect(m_customSpp, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RenderSettingsPanel::onCustomSppChanged);
    auto* customCaption = new QLabel(qualityGroup);
    qualityLayout->addRow(customCaption, m_customSpp);

    // A "Progressive rendering" checkbox used to sit here. It was connected to
    // nothing and read by nothing -- accumulation over frames is how the
    // renderer works, not a mode. Removed rather than wired up, same reasoning
    // as the read-only resolution group below.

    bindText([this, qualityGroup, targetCaption, customCaption] {
        qualityGroup->setTitle(tr("Quality"));
        targetCaption->setText(tr("Target samples:"));
        customCaption->setText(tr("Custom samples:"));

        const auto presets = catalog::qualityPresets();
        for (int i = 0; i < presets.size() && i < m_sppPreset->count(); ++i) {
            m_sppPreset->setItemText(i, catalog::qualityPresetLabel(presets.at(i)));
        }
        m_sppPreset->setItemText(m_sppPreset->count() - 1, tr("Custom..."));
    });

    mainLayout->addWidget(qualityGroup);

    // --- resolution -----------------------------------------------------
    // Every control here emits resolutionRequested() and nothing more: the
    // shell applies it through its one dispatcher and pushes the result back
    // through setResolutionPolicy(), so the Render menu and ql_set_resolution
    // cannot disagree with what these show.
    auto* resGroup = new QGroupBox(this);
    auto* resLayout = new QFormLayout(resGroup);

    using Mode = viewport::ResolutionPolicy::Mode;
    m_modeCombo = new QComboBox(resGroup);
    m_modeCombo->addItem(QString(), static_cast<int>(Mode::Fixed));
    m_modeCombo->addItem(QString(), static_cast<int>(Mode::Follow));
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RenderSettingsPanel::emitResolutionRequest);
    auto* modeCaption = new QLabel(resGroup);
    resLayout->addRow(modeCaption, m_modeCombo);

    m_presetCombo = new QComboBox(resGroup);
    for (const viewport::ResolutionPreset& preset : viewport::kResolutionPresets) {
        m_presetCombo->addItem(QStringLiteral("%1 x %2").arg(preset.width).arg(preset.height),
                               QSize(preset.width, preset.height));
    }
    m_presetCombo->addItem(QString(), QSize());   // custom: no size of its own
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RenderSettingsPanel::onPresetChosen);
    auto* presetCaption = new QLabel(resGroup);
    resLayout->addRow(presetCaption, m_presetCombo);

    m_widthSpin = new QSpinBox(resGroup);
    m_heightSpin = new QSpinBox(resGroup);
    for (QSpinBox* spin : {m_widthSpin, m_heightSpin}) {
        spin->setRange(viewport::kMinRenderSide, viewport::kMaxRenderSide);
        // Applied on commit, like the custom sample count: typing 1920 would
        // otherwise restart the render at 1, 19, 192 and 1920.
        spin->setKeyboardTracking(false);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &RenderSettingsPanel::onSizeEdited);
    }
    auto* widthCaption = new QLabel(resGroup);
    auto* heightCaption = new QLabel(resGroup);
    resLayout->addRow(widthCaption, m_widthSpin);
    resLayout->addRow(heightCaption, m_heightSpin);

    m_scaleCombo = new QComboBox(resGroup);
    for (const float scale : viewport::kFollowScales) {
        m_scaleCombo->addItem(QString::number(qRound(scale * 100)) + QLatin1Char('%'), scale);
    }
    connect(m_scaleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RenderSettingsPanel::emitResolutionRequest);
    auto* scaleCaption = new QLabel(resGroup);
    resLayout->addRow(scaleCaption, m_scaleCombo);

    m_fitCheck = new QCheckBox(resGroup);
    m_fitCheck->setChecked(true);
    connect(m_fitCheck, &QCheckBox::toggled, this, &RenderSettingsPanel::fitToViewToggled);
    resLayout->addRow(m_fitCheck);

    // The truth about the picture: what is traced, and how it is shown.
    m_resolutionLabel = new QLabel(resGroup);   // text from updateExtentLabel()
    auto* resCaption = new QLabel(resGroup);
    resLayout->addRow(resCaption, m_resolutionLabel);

    bindText([this, resGroup, modeCaption, presetCaption, widthCaption, heightCaption,
              scaleCaption, resCaption] {
        resGroup->setTitle(tr("Resolution"));
        modeCaption->setText(tr("Mode:"));
        m_modeCombo->setItemText(0, tr("Fixed resolution"));
        m_modeCombo->setItemText(1, tr("Follow viewport"));
        m_modeCombo->setToolTip(
            tr("Fixed renders at exactly the size below, however large the viewport is. "
               "Follow viewport renders at the viewport's pixel size times the scale."));
        presetCaption->setText(tr("Preset:"));
        m_presetCombo->setItemText(m_presetCombo->count() - 1, tr("Custom"));
        widthCaption->setText(tr("Width:"));
        heightCaption->setText(tr("Height:"));
        scaleCaption->setText(tr("Follow scale:"));
        m_fitCheck->setText(tr("Fit to view"));
        m_fitCheck->setToolTip(
            tr("On: the render is scaled to fit the viewport, up or down. Off: one render "
               "pixel per screen pixel, with scrollbars when it is larger than the viewport."));
        resCaption->setText(tr("Render resolution:"));
        m_resolutionLabel->setToolTip(
            tr("The size the renderer is tracing at now, and how much the viewport "
               "magnifies it (100% is one render pixel per screen pixel)."));
        updateExtentLabel();
    });

    mainLayout->addWidget(resGroup);

    // --- scene ----------------------------------------------------------
    // Document state, unlike the rest of this panel: [scene] vertex_colors.
    // Offered only where the SDK reports a Minecraft export carrying COLOR_0
    // -- the one kind of scene whose vertex colours are a deliberate tint
    // (grass, leaves, water) rather than whatever an exporter left there.
    auto* sceneGroup = new QGroupBox(this);
    auto* sceneLayout = new QFormLayout(sceneGroup);

    // Items from the catalogue, the same list Render ▸ Vertex Colours
    // builds, carrying the document's spelling as data.
    m_vertexColorCombo = new QComboBox(sceneGroup);
    for (const QString& id : catalog::vertexColorModeIds()) {
        m_vertexColorCombo->addItem(QString(), id);
    }
    m_vertexColorCombo->setCurrentIndex(
        std::max(0, m_vertexColorCombo->findData(QStringLiteral("auto"))));
    connect(m_vertexColorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (index < 0) return;
                emit vertexColorModeRequested(m_vertexColorCombo->itemData(index).toString());
            });
    auto* vertexColorCaption = new QLabel(sceneGroup);
    sceneLayout->addRow(vertexColorCaption, m_vertexColorCombo);

    m_vertexColorHint = new QLabel(sceneGroup);
    m_vertexColorHint->setWordWrap(true);
    bindStyle([this] { uistyle::applyHintStyle(m_vertexColorHint); });
    sceneLayout->addRow(m_vertexColorHint);

    bindText([this, sceneGroup, vertexColorCaption] {
        sceneGroup->setTitle(tr("Scene"));
        vertexColorCaption->setText(tr("Vertex colours:"));
        m_vertexColorCombo->setToolTip(
            tr("Whether the model's vertex colours (glTF COLOR_0) tint its surfaces. "
               "A Minecraft export keeps the biome tint of grass, leaves and water there. "
               "Changing it reloads the scene."));
        // By value: each item carries its identifier, so the labels follow
        // the language without moving the selection.
        for (int i = 0; i < m_vertexColorCombo->count(); ++i) {
            m_vertexColorCombo->setItemText(
                i, catalog::vertexColorModeLabel(m_vertexColorCombo->itemData(i).toString()));
        }
    });
    updateVertexColorHint();

    mainLayout->addWidget(sceneGroup);

    // --- display enhancement (hosted) -----------------------------------
    m_displayGroup = new QGroupBox(this);
    m_displayLayout = new QVBoxLayout(m_displayGroup);
    m_displayLayout->setContentsMargins(0, 0, 0, 0);
    bindText([this] { m_displayGroup->setTitle(tr("Display Enhancement")); });
    m_displayGroup->setVisible(false);   // shown once a widget is installed
    mainLayout->addWidget(m_displayGroup);

    // --- actions --------------------------------------------------------
    auto* actionsGroup = new QGroupBox(this);
    auto* actionsLayout = new QVBoxLayout(actionsGroup);

    m_resetBtn = new QPushButton(actionsGroup);
    connect(m_resetBtn, &QPushButton::clicked, this, &RenderSettingsPanel::onResetClicked);
    actionsLayout->addWidget(m_resetBtn);

    m_exportBtn = new QPushButton(actionsGroup);
    connect(m_exportBtn, &QPushButton::clicked, this, &RenderSettingsPanel::exportRequested);
    actionsLayout->addWidget(m_exportBtn);

    // Cheap now that the renderer stops itself at the target: the shell has a
    // moment where the render is provably finished, which is the only place an
    // unattended export could be written from.
    m_autoExportCheck = new QCheckBox(actionsGroup);
    m_autoExportCheck->setChecked(false);
    connect(m_autoExportCheck, &QCheckBox::toggled,
            this, &RenderSettingsPanel::autoExportToggled);
    actionsLayout->addWidget(m_autoExportCheck);

    bindText([this, actionsGroup] {
        actionsGroup->setTitle(tr("Actions"));
        m_resetBtn->setText(tr("Reset Accumulation"));
        m_resetBtn->setToolTip(tr("Clear accumulated samples and restart rendering"));
        m_exportBtn->setText(tr("Export Image..."));
        m_exportBtn->setToolTip(tr("Save the raw render, without display enhancement"));
        m_autoExportCheck->setText(tr("Export EXR when the target is reached"));
        m_autoExportCheck->setToolTip(
            tr("Writes beside the open configuration, named after it and the sample count. "
               "Has no effect in Infinite mode, which has no target to reach."));
    });

    mainLayout->addWidget(actionsGroup);
    mainLayout->addStretch();
}

bool RenderSettingsPanel::autoExportOnComplete() const {
    return m_autoExportCheck && m_autoExportCheck->isChecked();
}

void RenderSettingsPanel::setAutoExportOnComplete(bool enabled) {
    const QSignalBlocker blocker(m_autoExportCheck);
    m_autoExportCheck->setChecked(enabled);
}

QString RenderSettingsPanel::vertexColorMode() const {
    return m_vertexColorCombo->currentData().toString();
}

void RenderSettingsPanel::setVertexColorMode(const QString& mode) {
    const int index = m_vertexColorCombo->findData(mode);
    if (index < 0) return;
    const QSignalBlocker blocker(m_vertexColorCombo);
    m_vertexColorCombo->setCurrentIndex(index);
}

void RenderSettingsPanel::setVertexColorState(const quantiloom::SceneVertexColors* state) {
    if (state) {
        m_vertexColorState = *state;
    } else {
        m_vertexColorState.reset();
    }
    updateVertexColorHint();
}

bool RenderSettingsPanel::vertexColorsSelectable() const {
    return m_vertexColorState && m_vertexColorState->minecraftExport &&
           m_vertexColorState->present;
}

void RenderSettingsPanel::updateVertexColorHint() {
    const bool selectable = vertexColorsSelectable();
    m_vertexColorCombo->setEnabled(selectable);

    // Enabled, the line says what renders now; disabled, it says why.
    QString hint;
    if (!m_vertexColorState) {
        hint = tr("No scene loaded.");
    } else if (!m_vertexColorState->minecraftExport) {
        hint = tr("Only available for Minecraft scenes.");
    } else if (!m_vertexColorState->present) {
        hint = tr("This Minecraft scene carries no vertex colours (COLOR_0).");
    } else if (m_vertexColorState->active) {
        hint = tr("In effect: on (biome tint from COLOR_0)");
    } else {
        hint = tr("Not in effect");
    }
    m_vertexColorHint->setText(hint);
}

void RenderSettingsPanel::retranslateUi() {
    PanelBase::retranslateUi();
    setSampleCount(m_sampleCount);
    updateVertexColorHint();
}

void RenderSettingsPanel::setDisplayEnhancementWidget(QWidget* widget) {
    if (!widget) {
        return;
    }
    m_displayLayout->addWidget(widget);
    m_displayGroup->setVisible(true);
}

void RenderSettingsPanel::setSampleCount(uint32_t count) {
    // Against the target, so the number says how far along it is.
    m_sampleCount = count;
    m_sampleCountLabel->setText(m_targetSPP > 0
                                    ? tr("%1 / %2").arg(count).arg(m_targetSPP)
                                    : tr("%1 / ∞").arg(count));
}

void RenderSettingsPanel::setTargetSPP(uint32_t spp) {
    m_targetSPP = spp;
    setSampleCount(m_sampleCount);

    // Find matching preset (the last entry is "Custom...")
    for (int i = 0; i < m_sppPreset->count() - 1; ++i) {
        if (m_sppPreset->itemData(i).toUInt() == spp) {
            const QSignalBlocker blocker(m_sppPreset);
            m_sppPreset->setCurrentIndex(i);
            m_customSpp->setEnabled(false);
            return;
        }
    }

    {
        const QSignalBlocker blocker(m_sppPreset);
        m_sppPreset->setCurrentIndex(m_sppPreset->count() - 1);
    }
    m_customSpp->setEnabled(true);
    const QSignalBlocker blocker(m_customSpp);
    m_customSpp->setValue(static_cast<int>(spp));
}

void RenderSettingsPanel::setResolutionPolicy(const viewport::ResolutionPolicy& policy) {
    const QSignalBlocker modeBlocker(m_modeCombo);
    const QSignalBlocker widthBlocker(m_widthSpin);
    const QSignalBlocker heightBlocker(m_heightSpin);
    const QSignalBlocker scaleBlocker(m_scaleCombo);
    const bool follow = policy.mode == viewport::ResolutionPolicy::Mode::Follow;
    m_modeCombo->setCurrentIndex(m_modeCombo->findData(static_cast<int>(policy.mode)));
    m_widthSpin->setValue(policy.fixed.width());
    m_heightSpin->setValue(policy.fixed.height());
    selectPresetFor(policy.fixed);
    const int scaleIndex = m_scaleCombo->findData(policy.followScale);
    if (scaleIndex >= 0) {
        m_scaleCombo->setCurrentIndex(scaleIndex);
    }
    m_presetCombo->setEnabled(!follow);
    m_widthSpin->setEnabled(!follow);
    m_heightSpin->setEnabled(!follow);
    m_scaleCombo->setEnabled(follow);
}

void RenderSettingsPanel::setFitToView(bool fit) {
    const QSignalBlocker blocker(m_fitCheck);
    m_fitCheck->setChecked(fit);
}

void RenderSettingsPanel::setRenderExtent(const QSize& extent, double zoom) {
    m_extent = extent;
    m_zoom = zoom;
    updateExtentLabel();
}

void RenderSettingsPanel::updateExtentLabel() {
    if (m_extent.isEmpty()) {
        m_resolutionLabel->setText(QStringLiteral("-"));
        return;
    }
    m_resolutionLabel->setText(tr("%1 x %2  -  zoom %3%")
                                   .arg(m_extent.width())
                                   .arg(m_extent.height())
                                   .arg(qRound(m_zoom * 100.0)));
}

void RenderSettingsPanel::selectPresetFor(const QSize& size) {
    const QSignalBlocker blocker(m_presetCombo);
    const int index = m_presetCombo->findData(size);
    m_presetCombo->setCurrentIndex(index >= 0 ? index : m_presetCombo->count() - 1);
}

void RenderSettingsPanel::onPresetChosen(int index) {
    const QSize size = m_presetCombo->itemData(index).toSize();
    if (size.isEmpty()) {
        return;   // Custom keeps what the boxes say
    }
    {
        const QSignalBlocker widthBlocker(m_widthSpin);
        const QSignalBlocker heightBlocker(m_heightSpin);
        m_widthSpin->setValue(size.width());
        m_heightSpin->setValue(size.height());
    }
    emitResolutionRequest();
}

void RenderSettingsPanel::onSizeEdited() {
    selectPresetFor(QSize(m_widthSpin->value(), m_heightSpin->value()));
    emitResolutionRequest();
}

void RenderSettingsPanel::emitResolutionRequest() {
    viewport::ResolutionPolicy policy;
    policy.mode = static_cast<viewport::ResolutionPolicy::Mode>(m_modeCombo->currentData().toInt());
    policy.fixed = QSize(m_widthSpin->value(), m_heightSpin->value());
    policy.followScale = m_scaleCombo->currentData().toFloat();
    emit resolutionRequested(policy);
}

void RenderSettingsPanel::onSppPresetChanged(int index) {
    int spp = m_sppPreset->itemData(index).toInt();

    if (spp < 0) {
        m_customSpp->setEnabled(true);
        spp = m_customSpp->value();
    } else {
        m_customSpp->setEnabled(false);
        if (spp > 0) m_customSpp->setValue(spp);
    }

    m_targetSPP = static_cast<uint32_t>(spp);
    emit sppChanged(m_targetSPP);
}

void RenderSettingsPanel::onCustomSppChanged(int value) {
    if (m_customSpp->isEnabled()) {
        m_targetSPP = static_cast<uint32_t>(value);
        emit sppChanged(m_targetSPP);
    }
}

void RenderSettingsPanel::onResetClicked() {
    emit resetAccumulationRequested();
}
