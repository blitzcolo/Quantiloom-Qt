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

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QPushButton;
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

signals:
    void cameraEdited(const glm::vec3& position, const glm::vec3& target);
    void fovEdited(float fovYDegrees);
    void resetRequested();
    /// One of the six standard directions, as a unit vector from the target.
    void viewDirectionRequested(const glm::vec3& direction);

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

    bool m_updatingFields = false;
};
