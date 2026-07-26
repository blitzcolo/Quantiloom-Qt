/**
 * @file PropertiesPanel.hpp
 * @brief Contextual properties: edit whatever is currently selected
 *
 * Selecting a node used to leave the user to find the Material tab by hand,
 * and a node's transform could only be changed by dragging blind in the
 * viewport — there was nowhere to read a position, let alone type one. This
 * dock follows the selection: node selected shows its transform and a way in
 * to its material, material selected shows the material editor, nothing
 * selected says so.
 *
 * The numeric transform fields go through the same undo command as a gizmo
 * drag, so Ctrl+Z means the same thing whichever was used.
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <glm/glm.hpp>

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QStackedWidget;
QT_END_NAMESPACE

class MaterialEditorPanel;

class PropertiesPanel : public PanelBase {
    Q_OBJECT

public:
    /// @param materialEditor existing editor to host; ownership is taken over
    ///        by this panel's layout.
    explicit PropertiesPanel(MaterialEditorPanel* materialEditor, QWidget* parent = nullptr);

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("properties"); }
    void retranslateUi() override;

    void showEmptyState();
    void showNode(int nodeIndex, const QString& name,
                  const glm::mat4& transform, int materialIndex);
    void showMultipleSelection(int count);
    void showMaterial();

    /// Refresh the numbers without changing which page is shown; used while a
    /// gizmo drag is in progress.
    void updateNodeTransform(const glm::mat4& transform);

signals:
    /// A transform typed into the fields. The shell turns this into the same
    /// undo command a gizmo drag produces.
    void nodeTransformEdited(int nodeIndex, const glm::mat4& transform);
    void materialRequested(int materialIndex);

private slots:
    void onTransformFieldChanged();

private:
    void setupUi();
    void writeTransformFields(const glm::mat4& transform);
    [[nodiscard]] glm::mat4 readTransformFields() const;

    QStackedWidget* m_stack = nullptr;

    // Page 0: nothing selected
    QLabel* m_emptyLabel = nullptr;

    // Page 1: node
    QLabel* m_nodeName = nullptr;
    QGroupBox* m_transformGroup = nullptr;
    QLabel* m_positionCaption = nullptr;
    QLabel* m_rotationCaption = nullptr;
    QLabel* m_scaleCaption = nullptr;
    QDoubleSpinBox* m_position[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* m_rotation[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* m_scale[3] = {nullptr, nullptr, nullptr};
    QPushButton* m_editMaterialButton = nullptr;

    // Page 2: material
    MaterialEditorPanel* m_materialEditor = nullptr;

    // Page 3: several nodes
    QLabel* m_multiLabel = nullptr;

    int m_nodeIndex = -1;
    int m_materialIndex = -1;
    int m_multiCount = 0;
    bool m_updatingFields = false;
};
