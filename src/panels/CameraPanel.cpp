/**
 * @file CameraPanel.cpp
 */

#include "CameraPanel.hpp"

#include "../ui/UiStyle.hpp"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

CameraPanel::CameraPanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());
    setupUi();
}

QString CameraPanel::panelTitle() const {
    return tr("Camera");
}

void CameraPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // --- pose -------------------------------------------------------------
    m_poseGroup = new QGroupBox(this);
    auto* poseLayout = new QFormLayout(m_poseGroup);

    auto makeTriple = [this](QDoubleSpinBox* (&fields)[3]) {
        auto* row = new QHBoxLayout();
        for (int axis = 0; axis < 3; ++axis) {
            auto* spin = new QDoubleSpinBox();
            spin->setRange(-1e6, 1e6);
            spin->setDecimals(3);
            spin->setSingleStep(0.1);
            spin->setKeyboardTracking(false);
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &CameraPanel::onPoseFieldChanged);
            fields[axis] = spin;
            row->addWidget(spin);
        }
        return row;
    };

    m_positionCaption = new QLabel(m_poseGroup);
    poseLayout->addRow(m_positionCaption, makeTriple(m_position));

    m_targetCaption = new QLabel(m_poseGroup);
    poseLayout->addRow(m_targetCaption, makeTriple(m_target));

    m_distanceLabel = new QLabel(QStringLiteral("--"));
    m_distanceCaption = new QLabel(m_poseGroup);
    poseLayout->addRow(m_distanceCaption, m_distanceLabel);

    mainLayout->addWidget(m_poseGroup);

    // --- lens -------------------------------------------------------------
    m_lensGroup = new QGroupBox(this);
    auto* lensLayout = new QFormLayout(m_lensGroup);

    m_fovSpin = new QDoubleSpinBox();
    m_fovSpin->setRange(1.0, 170.0);
    m_fovSpin->setDecimals(1);
    m_fovSpin->setSingleStep(1.0);
    m_fovSpin->setKeyboardTracking(false);
    connect(m_fovSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CameraPanel::onFovChanged);
    m_fovCaption = new QLabel(m_lensGroup);
    lensLayout->addRow(m_fovCaption, m_fovSpin);

    mainLayout->addWidget(m_lensGroup);

    // --- presets ----------------------------------------------------------
    m_presetGroup = new QGroupBox(this);
    auto* presetLayout = new QGridLayout(m_presetGroup);

    struct Preset { const char* id; glm::vec3 direction; int row; int column; };
    static const Preset presets[] = {
        {"front",  { 0.0f,  0.0f,  1.0f}, 0, 0},
        {"back",   { 0.0f,  0.0f, -1.0f}, 0, 1},
        {"right",  { 1.0f,  0.0f,  0.0f}, 1, 0},
        {"left",   {-1.0f,  0.0f,  0.0f}, 1, 1},
        {"top",    { 0.0f,  1.0f,  0.0f}, 2, 0},
        {"bottom", { 0.0f, -1.0f,  0.0f}, 2, 1},
    };

    QList<QPushButton*> presetButtons;
    for (const Preset& preset : presets) {
        auto* button = new QPushButton(m_presetGroup);
        button->setProperty("presetId", QString::fromLatin1(preset.id));
        const glm::vec3 direction = preset.direction;
        connect(button, &QPushButton::clicked, this, [this, direction]() {
            emit viewDirectionRequested(direction);
        });
        presetLayout->addWidget(button, preset.row, preset.column);
        presetButtons.append(button);
    }

    m_resetButton = new QPushButton(m_presetGroup);
    connect(m_resetButton, &QPushButton::clicked, this, &CameraPanel::resetRequested);
    presetLayout->addWidget(m_resetButton, 3, 0, 1, 2);

    mainLayout->addWidget(m_presetGroup);

    auto* hint = new QLabel(this);
    uistyle::applyHintStyle(hint);
    mainLayout->addWidget(hint);
    mainLayout->addStretch();

    bindText([this, presetButtons, hint] {
        m_poseGroup->setTitle(tr("Pose"));
        m_positionCaption->setText(tr("Position:"));
        m_targetCaption->setText(tr("Look at:"));
        m_distanceCaption->setText(tr("Distance:"));

        m_lensGroup->setTitle(tr("Lens"));
        m_fovCaption->setText(tr("Vertical field of view:"));
        m_fovSpin->setSuffix(tr("°"));

        m_presetGroup->setTitle(tr("Views"));
        m_resetButton->setText(tr("Reset View"));
        for (QPushButton* button : presetButtons) {
            const QString id = button->property("presetId").toString();
            if (id == QLatin1String("front"))       button->setText(tr("Front"));
            else if (id == QLatin1String("back"))   button->setText(tr("Back"));
            else if (id == QLatin1String("right"))  button->setText(tr("Right"));
            else if (id == QLatin1String("left"))   button->setText(tr("Left"));
            else if (id == QLatin1String("top"))    button->setText(tr("Top"));
            else if (id == QLatin1String("bottom")) button->setText(tr("Bottom"));
        }

        hint->setText(tr("Right-drag orbits, middle-drag pans, the wheel zooms. "
                         "W/A/S/D fly the camera, Q/E move it down and up."));
    });
}

void CameraPanel::retranslateUi() {
    PanelBase::retranslateUi();
    // The distance readout carries a unit-formatted number, so it is rebuilt
    // rather than merely re-labelled.
    onPoseFieldChanged();
}

void CameraPanel::setCameraState(const glm::vec3& position, const glm::vec3& target,
                                 float fovYDegrees) {
    m_updatingFields = true;
    for (int axis = 0; axis < 3; ++axis) {
        m_position[axis]->setValue(position[axis]);
        m_target[axis]->setValue(target[axis]);
    }
    m_fovSpin->setValue(fovYDegrees);
    m_updatingFields = false;

    m_distanceLabel->setText(QString::number(glm::length(position - target), 'f', 3));
}

void CameraPanel::onPoseFieldChanged() {
    glm::vec3 position;
    glm::vec3 target;
    for (int axis = 0; axis < 3; ++axis) {
        position[axis] = static_cast<float>(m_position[axis]->value());
        target[axis] = static_cast<float>(m_target[axis]->value());
    }
    m_distanceLabel->setText(QString::number(glm::length(position - target), 'f', 3));

    if (m_updatingFields) {
        return;
    }
    emit cameraEdited(position, target);
}

void CameraPanel::onFovChanged(double value) {
    if (m_updatingFields) {
        return;
    }
    emit fovEdited(static_cast<float>(value));
}
