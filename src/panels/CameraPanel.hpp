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
    void setMotion(const quantiloom::camera::CameraMotionConfig& motion);
    void setCurrentTime(double seconds);
    void capturePose(const glm::vec3& position, const glm::vec3& target);

public slots:
    void addKeyframe();
    void updateKeyframe();
    void deleteKeyframe();

signals:
    void cameraEdited(const glm::vec3& position, const glm::vec3& target);
    void fovEdited(float fovYDegrees);
    void resetRequested();
    /// One of the six standard directions, as a unit vector from the target.
    void viewDirectionRequested(const glm::vec3& direction);
    void motionEdited(const quantiloom::camera::CameraMotionConfig& motion);
    void captureRequested();
    void previewTimeRequested(double seconds);

private slots:
    void onPoseFieldChanged();
    void onFovChanged(double value);

private:
    void setupUi();
    /// Refresh the derived readout without reporting an edit.
    void updateDistanceLabel();

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
