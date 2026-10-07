/**
 * @file CameraPanel.cpp
 */

#include "CameraPanel.hpp"

#include "../ui/UiStyle.hpp"

#include <QDoubleSpinBox>
#include <QComboBox>
#include <QSignalBlocker>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>

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
    poseLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);

    // Axis symbols, verbatim in every locale -- see PropertiesPanel.
    static const char* const kAxisNames[3] = {"X", "Y", "Z"};
    auto makeTriple = [this](QWidget* parent, QDoubleSpinBox* (&fields)[3],
                             double range, int decimals,
                             const std::function<void(QDoubleSpinBox*)>& wire) {
        auto* row = new QGridLayout();
        for (int axis = 0; axis < 3; ++axis) {
            auto* spin = new QDoubleSpinBox(parent);
            spin->setRange(-range, range);
            spin->setDecimals(decimals);
            spin->setKeyboardTracking(false);
            if (wire) wire(spin);
            fields[axis] = spin;
            row->addWidget(new QLabel(QString::fromLatin1(kAxisNames[axis]), parent),
                           axis, 0);
            row->addWidget(spin, axis, 1);
        }
        return row;
    };
    auto wirePoseField = [this](QDoubleSpinBox* spin) {
        spin->setSingleStep(0.1);
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &CameraPanel::onPoseFieldChanged);
    };

    m_positionCaption = new QLabel(m_poseGroup);
    poseLayout->addRow(m_positionCaption,
                       makeTriple(m_poseGroup, m_position, 1e6, 3, wirePoseField));

    m_targetCaption = new QLabel(m_poseGroup);
    poseLayout->addRow(m_targetCaption,
                       makeTriple(m_poseGroup, m_target, 1e6, 3, wirePoseField));

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

    m_motionGroup = new QGroupBox(this);
    auto* motionLayout = new QFormLayout(m_motionGroup);
    motionLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    m_keyList = new QComboBox(m_motionGroup);
    connect(m_keyList, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (index < 0 || index >= static_cast<int>(m_motion.keys.size())) return;
                const auto& key = m_motion.keys[static_cast<size_t>(index)];
                m_updatingFields = true;
                m_keyTime->setValue(key.timeSeconds);
                for (int axis = 0; axis < 3; ++axis) {
                    m_keyPosition[axis]->setValue(key.position[axis]);
                    m_keyTarget[axis]->setValue(key.lookAt[axis]);
                }
                m_updatingFields = false;
                emit previewTimeRequested(key.timeSeconds);
            });
    motionLayout->addRow(m_keyList);
    m_keyTime = new QDoubleSpinBox(m_motionGroup);
    m_keyTime->setRange(-1e9, 1e9);
    // Fifteen decimals is not precision theatre: QDoubleSpinBox rounds its
    // value to its decimals, and the key fields are read back into the
    // trajectory, so fewer would quantise authored key times and positions.
    m_keyTime->setDecimals(15);
    m_keyTime->setKeyboardTracking(false);
    m_keyTimeCaption = new QLabel(m_motionGroup);
    motionLayout->addRow(m_keyTimeCaption, m_keyTime);
    m_keyPositionCaption = new QLabel(m_motionGroup);
    m_keyTargetCaption = new QLabel(m_motionGroup);
    motionLayout->addRow(m_keyPositionCaption,
                         makeTriple(m_motionGroup, m_keyPosition, 1e9, 15, {}));
    motionLayout->addRow(m_keyTargetCaption,
                         makeTriple(m_motionGroup, m_keyTarget, 1e9, 15, {}));
    auto* keyButtons = new QGridLayout();
    m_captureButton = new QPushButton(m_motionGroup);
    m_addButton = new QPushButton(m_motionGroup);
    m_updateButton = new QPushButton(m_motionGroup);
    m_deleteButton = new QPushButton(m_motionGroup);
    keyButtons->addWidget(m_captureButton, 0, 0);
    keyButtons->addWidget(m_addButton, 0, 1);
    keyButtons->addWidget(m_updateButton, 1, 0);
    keyButtons->addWidget(m_deleteButton, 1, 1);
    motionLayout->addRow(keyButtons);
    connect(m_captureButton, &QPushButton::clicked, this, &CameraPanel::captureRequested);
    connect(m_addButton, &QPushButton::clicked, this, &CameraPanel::addKeyframe);
    connect(m_updateButton, &QPushButton::clicked, this, &CameraPanel::updateKeyframe);
    connect(m_deleteButton, &QPushButton::clicked, this, &CameraPanel::deleteKeyframe);
    connect(m_keyTime, &QDoubleSpinBox::editingFinished, this, &CameraPanel::updateKeyframe);
    for (int axis = 0; axis < 3; ++axis) {
        connect(m_keyPosition[axis], &QDoubleSpinBox::editingFinished,
                this, &CameraPanel::updateKeyframe);
        connect(m_keyTarget[axis], &QDoubleSpinBox::editingFinished,
                this, &CameraPanel::updateKeyframe);
    }
    mainLayout->addWidget(m_motionGroup);

    auto* hint = new QLabel(this);
    bindStyle([hint] { uistyle::applyHintStyle(hint); });
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
        m_motionGroup->setTitle(tr("Camera trajectory"));
        m_keyTimeCaption->setText(tr("Key time:"));
        m_keyPositionCaption->setText(tr("Key position:"));
        m_keyTargetCaption->setText(tr("Key look at:"));
        m_captureButton->setText(tr("Capture pose"));
        m_addButton->setText(tr("Add key"));
        m_updateButton->setText(tr("Update key"));
        m_deleteButton->setText(tr("Delete key"));
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
    // Rebuild the formatted reading, but through the display-only helper.
    // Calling the field handler here emitted cameraEdited, so merely switching
    // language moved the camera, reset the accumulation and marked the scene
    // modified -- the application then asked to save on exit.
    updateDistanceLabel();
}

void CameraPanel::setCameraState(const glm::vec3& position, const glm::vec3& target,
                                 float fovYDegrees) {
    m_updatingFields = true;
    for (int axis = 0; axis < 3; ++axis) {
        m_position[axis]->setValue(position[axis]);
        m_target[axis]->setValue(target[axis]);
    }
    m_fovSpin->setValue(fovYDegrees);
    if (m_motion.keys.empty()) {
        m_keyTime->setValue(m_currentTime);
        for (int axis = 0; axis < 3; ++axis) {
            m_keyPosition[axis]->setValue(position[axis]);
            m_keyTarget[axis]->setValue(target[axis]);
        }
    }
    m_updatingFields = false;

    m_distanceLabel->setText(QString::number(glm::length(position - target), 'f', 3));
}

void CameraPanel::updateDistanceLabel() {
    glm::vec3 position;
    glm::vec3 target;
    for (int axis = 0; axis < 3; ++axis) {
        position[axis] = static_cast<float>(m_position[axis]->value());
        target[axis] = static_cast<float>(m_target[axis]->value());
    }
    m_distanceLabel->setText(QString::number(glm::length(position - target), 'f', 3));
}

void CameraPanel::onPoseFieldChanged() {
    updateDistanceLabel();

    if (m_updatingFields) {
        return;
    }

    glm::vec3 position;
    glm::vec3 target;
    for (int axis = 0; axis < 3; ++axis) {
        position[axis] = static_cast<float>(m_position[axis]->value());
        target[axis] = static_cast<float>(m_target[axis]->value());
    }
    emit cameraEdited(position, target);
}

void CameraPanel::onFovChanged(double value) {
    if (m_updatingFields) {
        return;
    }
    emit fovEdited(static_cast<float>(value));
}

void CameraPanel::setMotion(const quantiloom::camera::CameraMotionConfig& motion) {
    int selected = m_keyList->currentIndex();
    m_motion = motion;
    const bool previewEditedKey = m_selectAfterEdit.has_value();
    if (m_selectAfterEdit) {
        const auto at = std::find_if(m_motion.keys.begin(), m_motion.keys.end(),
            [this](const auto& key) { return key.timeSeconds == *m_selectAfterEdit; });
        if (at != m_motion.keys.end())
            selected = static_cast<int>(std::distance(m_motion.keys.begin(), at));
        m_selectAfterEdit.reset();
    }
    const QSignalBlocker block(m_keyList);
    m_keyList->clear();
    for (const auto& key : m_motion.keys)
        m_keyList->addItem(QString::number(key.timeSeconds, 'g', 12) + QStringLiteral(" s"));
    m_keyList->setCurrentIndex(m_motion.keys.empty() ? -1 :
        std::clamp(selected, 0, static_cast<int>(m_motion.keys.size()) - 1));
    if (m_keyList->currentIndex() >= 0) {
        const auto& key = m_motion.keys[static_cast<size_t>(m_keyList->currentIndex())];
        m_updatingFields = true;
        m_keyTime->setValue(key.timeSeconds);
        for (int axis = 0; axis < 3; ++axis) {
            m_keyPosition[axis]->setValue(key.position[axis]);
            m_keyTarget[axis]->setValue(key.lookAt[axis]);
        }
        m_updatingFields = false;
        if (previewEditedKey) emit previewTimeRequested(key.timeSeconds);
    }
}

void CameraPanel::setCurrentTime(double seconds) {
    m_currentTime = seconds;
    if (m_keyList->currentIndex() < 0) m_keyTime->setValue(seconds);
}

void CameraPanel::capturePose(const glm::vec3& position, const glm::vec3& target) {
    for (int axis = 0; axis < 3; ++axis) {
        m_keyPosition[axis]->setValue(position[axis]);
        m_keyTarget[axis]->setValue(target[axis]);
    }
    m_keyTime->setValue(m_currentTime);
    if (m_keyList->currentIndex() < 0) addKeyframe();
    else updateKeyframe();
}

quantiloom::camera::CameraPoseKey CameraPanel::keyFromFields(double timeSeconds) const {
    quantiloom::camera::CameraPoseKey key;
    key.timeSeconds = timeSeconds;
    for (int axis = 0; axis < 3; ++axis) {
        key.position[axis] = m_keyPosition[axis]->value();
        key.lookAt[axis] = m_keyTarget[axis]->value();
    }
    return key;
}

void CameraPanel::insertSorted(quantiloom::camera::CameraMotionConfig& motion,
                               const quantiloom::camera::CameraPoseKey& key) {
    const auto at = std::lower_bound(motion.keys.begin(), motion.keys.end(),
        key.timeSeconds,
        [](const auto& entry, double time) { return entry.timeSeconds < time; });
    motion.keys.insert(at, key);
}

void CameraPanel::addKeyframe() {
    quantiloom::camera::CameraMotionConfig next = m_motion;
    // Adding follows the transport grid. The selected key's numeric time is
    // for editing that key; reusing it here would create a duplicate.
    const auto key = keyFromFields(m_currentTime);
    insertSorted(next, key);
    m_selectAfterEdit = key.timeSeconds;
    emit motionEdited(next);
}

void CameraPanel::updateKeyframe() {
    if (m_updatingFields) return;
    const int selected = m_keyList->currentIndex();
    if (selected < 0 || selected >= static_cast<int>(m_motion.keys.size())) return;
    quantiloom::camera::CameraMotionConfig next = m_motion;
    next.keys.erase(next.keys.begin() + selected);
    const auto key = keyFromFields(m_keyTime->value());
    insertSorted(next, key);
    m_selectAfterEdit = key.timeSeconds;
    emit motionEdited(next);
}

void CameraPanel::deleteKeyframe() {
    const int selected = m_keyList->currentIndex();
    if (selected < 0 || selected >= static_cast<int>(m_motion.keys.size())) return;
    quantiloom::camera::CameraMotionConfig next = m_motion;
    next.keys.erase(next.keys.begin() + selected);
    emit motionEdited(next);
}
