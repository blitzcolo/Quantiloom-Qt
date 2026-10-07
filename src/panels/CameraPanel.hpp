/**
 * @file CameraPanel.hpp
 * @brief Numeric camera pose and field of view
 *
 * The camera is the object the user touches most and was the only first-class
 * parameter with no panel at all: orbit, pan and zoom lived entirely in the
 * mouse, and the position and field of view existed only as TOML fields fed
 * straight to the viewport. This shows them, lets them be typed, and stays in
 * step with the mouse.
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <glm/glm.hpp>
#include <postprocess/CameraPipeline.hpp>
#include <optional>

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QComboBox;
QT_END_NAMESPACE

class CameraPanel : public PanelBase {
    Q_OBJECT

public:
    explicit CameraPanel(QWidget* parent = nullptr);

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("camera"); }
    void retranslateUi() override;

    /// Show a pose that came from somewhere else (mouse, config, preset).
    /// Does not emit.
    void setCameraState(const glm::vec3& position, const glm::vec3& target, float fovYDegrees);
    /// Show a motion that came from the document or the renderer. Restores
    /// the selection to the key an edit last touched when one is pending
    /// (m_selectAfterEdit), so a round trip through the renderer does not
    /// appear to move the user's selection. Does not emit motionEdited.
    void setMotion(const quantiloom::camera::CameraMotionConfig& motion);
    /// Where the transport stands. "Add key" creates its key at this time,
    /// which is what makes capture/add land where the user is looking rather
    /// than at whatever the time field last held.
    void setCurrentTime(double seconds);
    /// Fill the key fields with a pose the host captured -- the viewport's
    /// camera -- at the current time, then commit it: update the selected
    /// key if there is one, add a new key otherwise.
    void capturePose(const glm::vec3& position, const glm::vec3& target);

public slots:
    /// Key the field pose at the transport's time. The time field belongs to
    /// the selected key; reusing it here would duplicate an existing key.
    void addKeyframe();
    /// Replace the selected key with the fields. Wired to editingFinished as
    /// well as its button, so retuning a field is not lost to the list entry.
    void updateKeyframe();
    void deleteKeyframe();

signals:
    void cameraEdited(const glm::vec3& position, const glm::vec3& target);
    void fovEdited(float fovYDegrees);
    void resetRequested();
    /// One of the six standard directions, as a unit vector from the target.
    void viewDirectionRequested(const glm::vec3& direction);
    /// The whole motion after each edit: the keys are the document's
    /// camera.motion, and a partially edited trajectory has no meaning to
    /// emit.
    void motionEdited(const quantiloom::camera::CameraMotionConfig& motion);
    /// The Capture button asking for the viewport's pose -- the panel owns
    /// widgets, not a camera, so the host answers this and calls
    /// capturePose() with the result.
    void captureRequested();
    /// Selecting a key asks the host to move the clock to it, so the
    /// viewport shows the pose being edited.
    void previewTimeRequested(double seconds);

private slots:
    void onPoseFieldChanged();
    void onFovChanged(double value);

private:
    void setupUi();
    /// Refresh the derived readout without reporting an edit.
    void updateDistanceLabel();
    /// The key the six key fields currently describe, at the given time.
    [[nodiscard]] quantiloom::camera::CameraPoseKey keyFromFields(double timeSeconds) const;
    /// Keys stay sorted by time; an edited key may belong anywhere in the list.
    static void insertSorted(quantiloom::camera::CameraMotionConfig& motion,
                             const quantiloom::camera::CameraPoseKey& key);

    QGroupBox* m_poseGroup = nullptr;
    QLabel* m_positionCaption = nullptr;
    QLabel* m_targetCaption = nullptr;
    QDoubleSpinBox* m_position[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* m_target[3] = {nullptr, nullptr, nullptr};

    QGroupBox* m_lensGroup = nullptr;
    QLabel* m_fovCaption = nullptr;
    QDoubleSpinBox* m_fovSpin = nullptr;
    QLabel* m_distanceLabel = nullptr;
    QLabel* m_distanceCaption = nullptr;

    QGroupBox* m_presetGroup = nullptr;
    QPushButton* m_resetButton = nullptr;

    QGroupBox* m_motionGroup = nullptr;
    QComboBox* m_keyList = nullptr;
    QDoubleSpinBox* m_keyTime = nullptr;
    QDoubleSpinBox* m_keyPosition[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* m_keyTarget[3] = {nullptr, nullptr, nullptr};
    QPushButton* m_captureButton = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_updateButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QLabel* m_keyTimeCaption = nullptr;
    QLabel* m_keyPositionCaption = nullptr;
    QLabel* m_keyTargetCaption = nullptr;
    quantiloom::camera::CameraMotionConfig m_motion;
    double m_currentTime = 0.0;
    std::optional<double> m_selectAfterEdit;

    bool m_updatingFields = false;
};
